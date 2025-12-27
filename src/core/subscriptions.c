// subscriptions.c - Subscription Management for Streaming Payments
// Monthly/yearly subscriptions with auto-renewal and cancellation

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#define MAX_SUBSCRIPTIONS 1000
#define SUBSCRIPTION_FILE "subscriptions.dat"
#define SUBSCRIPTION_MAGIC 0x53554253  // "SUBS"

// Subscription types
typedef enum {
    SUB_MONTHLY = 1,
    SUB_YEARLY = 2,
    SUB_CUSTOM = 3
} SubscriptionType;

// Subscription status
typedef enum {
    SUB_ACTIVE = 1,
    SUB_PAUSED = 2,
    SUB_CANCELLED = 3,
    SUB_EXPIRED = 4
} SubscriptionStatus;

// Subscription plan
typedef struct {
    uint64_t plan_id;
    char name[128];
    char description[256];
    double price;
    uint32_t duration_seconds;  // Billing period
    SubscriptionType type;
    uint8_t provider_pubkey[32];
    int active;
} SubscriptionPlan;

// Active subscription
typedef struct {
    uint64_t subscription_id;
    uint64_t plan_id;
    uint64_t stream_id;  // Linked payment stream
    uint8_t subscriber_pubkey[32];
    uint8_t provider_pubkey[32];
    uint64_t started_at;
    uint64_t next_billing;
    uint64_t cancelled_at;
    double price;
    uint32_t billing_period;
    SubscriptionStatus status;
    uint32_t payment_failures;
    uint8_t authorization_sig[64];
} Subscription;

// Registry
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t num_plans;
    uint32_t num_subscriptions;
    SubscriptionPlan plans[MAX_SUBSCRIPTIONS];
    Subscription subscriptions[MAX_SUBSCRIPTIONS];
    uint64_t next_plan_id;
    uint64_t next_sub_id;
} SubscriptionRegistry;

static SubscriptionRegistry g_sub_registry = {0};
static int g_sub_initialized = 0;

// Save to disk
static PCError save_subscriptions(void) {
    FILE* f = fopen(SUBSCRIPTION_FILE ".tmp", "wb");
    if (!f) return PC_ERR_IO;
    
    g_sub_registry.magic = SUBSCRIPTION_MAGIC;
    g_sub_registry.version = 1;
    
    fwrite(&g_sub_registry, sizeof(SubscriptionRegistry), 1, f);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    
    if (rename(SUBSCRIPTION_FILE ".tmp", SUBSCRIPTION_FILE) != 0) {
        return PC_ERR_IO;
    }
    
    return PC_OK;
}

// Load from disk
static PCError load_subscriptions(void) {
    FILE* f = fopen(SUBSCRIPTION_FILE, "rb");
    if (!f) {
        memset(&g_sub_registry, 0, sizeof(SubscriptionRegistry));
        g_sub_registry.magic = SUBSCRIPTION_MAGIC;
        g_sub_registry.version = 1;
        g_sub_registry.next_plan_id = 1;
        g_sub_registry.next_sub_id = 1;
        g_sub_initialized = 1;
        return PC_OK;
    }
    
    if (fread(&g_sub_registry, sizeof(SubscriptionRegistry), 1, f) != 1) {
        fclose(f);
        return PC_ERR_IO;
    }
    
    fclose(f);
    
    if (g_sub_registry.magic != SUBSCRIPTION_MAGIC) {
        return PC_ERR_IO;
    }
    
    g_sub_initialized = 1;
    
    printf("Loaded %u subscription plans, %u active subscriptions\n",
           g_sub_registry.num_plans, g_sub_registry.num_subscriptions);
    
    return PC_OK;
}

// Initialize
void sub_init(void) {
    if (!g_sub_initialized) {
        load_subscriptions();
    }
}

// Create subscription plan
uint64_t sub_create_plan(const uint8_t* provider_pubkey, const char* name,
                         const char* description, double price,
                         SubscriptionType type) {
    if (!provider_pubkey || !name || price <= 0) return 0;
    
    if (g_sub_registry.num_plans >= MAX_SUBSCRIPTIONS) return 0;
    
    SubscriptionPlan* plan = &g_sub_registry.plans[g_sub_registry.num_plans];
    
    plan->plan_id = g_sub_registry.next_plan_id++;
    strncpy(plan->name, name, 127);
    plan->name[127] = '\0';
    
    if (description) {
        strncpy(plan->description, description, 255);
        plan->description[255] = '\0';
    }
    
    plan->price = price;
    plan->type = type;
    memcpy(plan->provider_pubkey, provider_pubkey, 32);
    plan->active = 1;
    
    // Set billing period
    switch (type) {
        case SUB_MONTHLY:
            plan->duration_seconds = 30 * 24 * 3600;  // 30 days
            break;
        case SUB_YEARLY:
            plan->duration_seconds = 365 * 24 * 3600;  // 365 days
            break;
        case SUB_CUSTOM:
            plan->duration_seconds = 24 * 3600;  // Default 1 day
            break;
    }
    
    g_sub_registry.num_plans++;
    save_subscriptions();
    
    printf("Created subscription plan #%lu: %s (%.2f per period)\n",
           plan->plan_id, name, price);
    
    return plan->plan_id;
}

// Get plan
static SubscriptionPlan* get_plan(uint64_t plan_id) {
    for (uint32_t i = 0; i < g_sub_registry.num_plans; i++) {
        if (g_sub_registry.plans[i].plan_id == plan_id && 
            g_sub_registry.plans[i].active) {
            return &g_sub_registry.plans[i];
        }
    }
    return NULL;
}

// Subscribe to plan
uint64_t sub_subscribe(uint64_t plan_id, const uint8_t* subscriber_pubkey,
                       uint64_t stream_id, const uint8_t* authorization_sig) {
    if (!subscriber_pubkey) return 0;
    
    SubscriptionPlan* plan = get_plan(plan_id);
    if (!plan) return 0;
    
    if (g_sub_registry.num_subscriptions >= MAX_SUBSCRIPTIONS) return 0;
    
    Subscription* sub = &g_sub_registry.subscriptions[g_sub_registry.num_subscriptions];
    
    sub->subscription_id = g_sub_registry.next_sub_id++;
    sub->plan_id = plan_id;
    sub->stream_id = stream_id;
    memcpy(sub->subscriber_pubkey, subscriber_pubkey, 32);
    memcpy(sub->provider_pubkey, plan->provider_pubkey, 32);
    sub->started_at = time(NULL);
    sub->next_billing = sub->started_at + plan->duration_seconds;
    sub->cancelled_at = 0;
    sub->price = plan->price;
    sub->billing_period = plan->duration_seconds;
    sub->status = SUB_ACTIVE;
    sub->payment_failures = 0;
    
    if (authorization_sig) {
        memcpy(sub->authorization_sig, authorization_sig, 64);
    }
    
    g_sub_registry.num_subscriptions++;
    save_subscriptions();
    
    printf("Subscription #%lu created for plan '%s'\n", 
           sub->subscription_id, plan->name);
    
    return sub->subscription_id;
}

// Get subscription
static Subscription* get_subscription(uint64_t sub_id) {
    for (uint32_t i = 0; i < g_sub_registry.num_subscriptions; i++) {
        if (g_sub_registry.subscriptions[i].subscription_id == sub_id) {
            return &g_sub_registry.subscriptions[i];
        }
    }
    return NULL;
}

// Cancel subscription
PCError sub_cancel(uint64_t sub_id) {
    Subscription* sub = get_subscription(sub_id);
    if (!sub) return PC_ERR_WALLET_NOT_FOUND;
    
    if (sub->status == SUB_CANCELLED) {
        return PC_OK;  // Already cancelled
    }
    
    sub->status = SUB_CANCELLED;
    sub->cancelled_at = time(NULL);
    
    save_subscriptions();
    
    printf("Subscription #%lu cancelled\n", sub_id);
    
    return PC_OK;
}

// Pause subscription
PCError sub_pause(uint64_t sub_id) {
    Subscription* sub = get_subscription(sub_id);
    if (!sub) return PC_ERR_WALLET_NOT_FOUND;
    
    if (sub->status != SUB_ACTIVE) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    sub->status = SUB_PAUSED;
    
    save_subscriptions();
    
    printf("Subscription #%lu paused\n", sub_id);
    
    return PC_OK;
}

// Resume subscription
PCError sub_resume(uint64_t sub_id) {
    Subscription* sub = get_subscription(sub_id);
    if (!sub) return PC_ERR_WALLET_NOT_FOUND;
    
    if (sub->status != SUB_PAUSED) {
        return PC_ERR_INVALID_SIGNATURE;
    }
    
    sub->status = SUB_ACTIVE;
    // Reset next billing to now + period
    sub->next_billing = time(NULL) + sub->billing_period;
    
    save_subscriptions();
    
    printf("Subscription #%lu resumed\n", sub_id);
    
    return PC_OK;
}

// Process billing for subscriptions
PCError sub_process_billing(PCState* state) {
    if (!state) return PC_ERR_IO;
    
    uint64_t now = time(NULL);
    uint32_t processed = 0;
    uint32_t failed = 0;
    
    for (uint32_t i = 0; i < g_sub_registry.num_subscriptions; i++) {
        Subscription* sub = &g_sub_registry.subscriptions[i];
        
        // Skip if not active or not due
        if (sub->status != SUB_ACTIVE) continue;
        if (now < sub->next_billing) continue;
        
        // Find wallets
        PCWallet* subscriber = pc_state_get_wallet(state, sub->subscriber_pubkey);
        PCWallet* provider = pc_state_get_wallet(state, sub->provider_pubkey);
        
        if (!subscriber || !provider) {
            sub->payment_failures++;
            if (sub->payment_failures >= 3) {
                sub->status = SUB_EXPIRED;
            }
            failed++;
            continue;
        }
        
        // Check if subscriber has funds
        if (subscriber->energy < sub->price) {
            sub->payment_failures++;
            if (sub->payment_failures >= 3) {
                sub->status = SUB_EXPIRED;
                printf("Subscription #%lu expired due to insufficient funds\n", 
                       sub->subscription_id);
            }
            failed++;
            continue;
        }
        
        // Execute payment
        double before_sum = subscriber->energy + provider->energy;
        subscriber->energy -= sub->price;
        provider->energy += sub->price;
        double after_sum = subscriber->energy + provider->energy;
        
        // Verify conservation
        if (fabs(before_sum - after_sum) > 1e-12) {
            // Rollback
            subscriber->energy += sub->price;
            provider->energy -= sub->price;
            sub->payment_failures++;
            failed++;
            continue;
        }
        
        // Success - update subscription
        subscriber->nonce++;
        sub->next_billing = now + sub->billing_period;
        sub->payment_failures = 0;
        processed++;
        
        printf("Subscription #%lu billed: %.2f coins\n", 
               sub->subscription_id, sub->price);
    }
    
    if (processed > 0 || failed > 0) {
        save_subscriptions();
        printf("Billing complete: %u processed, %u failed\n", processed, failed);
    }
    
    return PC_OK;
}

// List all plans
void sub_list_plans(void) {
    printf("\nSubscription Plans:\n");
    printf("┌────────┬──────────────────────────┬────────────┬──────────┐\n");
    printf("│ ID     │ Name                     │ Price      │ Period   │\n");
    printf("├────────┼──────────────────────────┼────────────┼──────────┤\n");
    
    for (uint32_t i = 0; i < g_sub_registry.num_plans; i++) {
        SubscriptionPlan* p = &g_sub_registry.plans[i];
        if (!p->active) continue;
        
        const char* period = "Custom";
        switch (p->type) {
            case SUB_MONTHLY: period = "Monthly"; break;
            case SUB_YEARLY: period = "Yearly"; break;
            case SUB_CUSTOM: period = "Custom"; break;
        }
        
        printf("│ %-6lu │ %-24s │ %10.2f │ %-8s │\n",
               p->plan_id, p->name, p->price, period);
    }
    
    printf("└────────┴──────────────────────────┴────────────┴──────────┘\n");
}

// List subscriptions
void sub_list_subscriptions(void) {
    printf("\nActive Subscriptions:\n");
    printf("┌────────┬─────────┬────────────┬───────────────────┐\n");
    printf("│ ID     │ Plan    │ Price      │ Next Billing      │\n");
    printf("├────────┼─────────┼────────────┼───────────────────┤\n");
    
    for (uint32_t i = 0; i < g_sub_registry.num_subscriptions; i++) {
        Subscription* s = &g_sub_registry.subscriptions[i];
        
        const char* status = "???";
        switch (s->status) {
            case SUB_ACTIVE: status = "ACTIVE"; break;
            case SUB_PAUSED: status = "PAUSED"; break;
            case SUB_CANCELLED: status = "CANCELLED"; break;
            case SUB_EXPIRED: status = "EXPIRED"; break;
        }
        
        char next_billing[20] = "N/A";
        if (s->status == SUB_ACTIVE) {
            uint64_t seconds_until = s->next_billing - time(NULL);
            if (seconds_until < 3600) {
                snprintf(next_billing, sizeof(next_billing), "%lum", 
                        (unsigned long)(seconds_until / 60));
            } else if (seconds_until < 86400) {
                snprintf(next_billing, sizeof(next_billing), "%luh", 
                        (unsigned long)(seconds_until / 3600));
            } else {
                snprintf(next_billing, sizeof(next_billing), "%lud", 
                        (unsigned long)(seconds_until / 86400));
            }
        }
        
        printf("│ %-6lu │ %-7lu │ %10.2f │ %-17s │\n",
               s->subscription_id, s->plan_id, s->price, next_billing);
    }
    
    printf("└────────┴─────────┴────────────┴───────────────────┘\n");
}

// Get subscription info
void sub_info(uint64_t sub_id) {
    Subscription* sub = get_subscription(sub_id);
    if (!sub) {
        printf("Subscription not found\n");
        return;
    }
    
    SubscriptionPlan* plan = get_plan(sub->plan_id);
    
    printf("\nSubscription #%lu:\n", sub_id);
    printf("  Plan: %s\n", plan ? plan->name : "Unknown");
    printf("  Price: %.2f per billing period\n", sub->price);
    printf("  Status: ");
    switch (sub->status) {
        case SUB_ACTIVE: printf("ACTIVE\n"); break;
        case SUB_PAUSED: printf("PAUSED\n"); break;
        case SUB_CANCELLED: printf("CANCELLED\n"); break;
        case SUB_EXPIRED: printf("EXPIRED\n"); break;
    }
    printf("  Started: %lu\n", sub->started_at);
    printf("  Next billing: %lu\n", sub->next_billing);
    printf("  Payment failures: %u\n", sub->payment_failures);
}

// Cleanup expired subscriptions
void sub_cleanup(void) {
    uint32_t removed = 0;
    
    for (uint32_t i = 0; i < g_sub_registry.num_subscriptions; ) {
        if (g_sub_registry.subscriptions[i].status == SUB_EXPIRED ||
            g_sub_registry.subscriptions[i].status == SUB_CANCELLED) {
            // Shift remaining subscriptions
            memmove(&g_sub_registry.subscriptions[i],
                    &g_sub_registry.subscriptions[i + 1],
                    (g_sub_registry.num_subscriptions - i - 1) * sizeof(Subscription));
            g_sub_registry.num_subscriptions--;
            removed++;
        } else {
            i++;
        }
    }
    
    if (removed > 0) {
        save_subscriptions();
        printf("Cleaned up %u expired/cancelled subscriptions\n", removed);
    }
}

