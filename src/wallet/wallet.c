// wallet.c - HD Wallet with Mnemonic Backup
// BIP39-style mnemonic generation and wallet derivation

#include "../include/physicscoin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sodium.h>

// Simplified BIP39 wordlist (first 256 words for 8-bit entropy)
static const char* WORDLIST[256] = {
    "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract",
    "absurd", "abuse", "access", "accident", "account", "accuse", "achieve", "acid",
    "acoustic", "acquire", "across", "act", "action", "actor", "actress", "actual",
    "adapt", "add", "addict", "address", "adjust", "admit", "adult", "advance",
    "advice", "aerobic", "affair", "afford", "afraid", "again", "age", "agent",
    "agree", "ahead", "aim", "air", "airport", "aisle", "alarm", "album",
    "alcohol", "alert", "alien", "all", "alley", "allow", "almost", "alone",
    "alpha", "already", "also", "alter", "always", "amateur", "amazing", "among",
    "amount", "amused", "analyst", "anchor", "ancient", "anger", "angle", "angry",
    "animal", "ankle", "announce", "annual", "another", "answer", "antenna", "antique",
    "anxiety", "any", "apart", "apology", "appear", "apple", "approve", "april",
    "arch", "arctic", "area", "arena", "argue", "arm", "armed", "armor",
    "army", "around", "arrange", "arrest", "arrive", "arrow", "art", "artefact",
    "artist", "artwork", "ask", "aspect", "assault", "asset", "assist", "assume",
    "asthma", "athlete", "atom", "attack", "attend", "attitude", "attract", "auction",
    "audit", "august", "aunt", "author", "auto", "autumn", "average", "avocado",
    "avoid", "awake", "aware", "away", "awesome", "awful", "awkward", "axis",
    "baby", "bachelor", "bacon", "badge", "bag", "balance", "balcony", "ball",
    "bamboo", "banana", "banner", "bar", "barely", "bargain", "barrel", "base",
    "basic", "basket", "battle", "beach", "bean", "beauty", "because", "become",
    "beef", "before", "begin", "behave", "behind", "believe", "below", "belt",
    "bench", "benefit", "best", "betray", "better", "between", "beyond", "bicycle",
    "bid", "bike", "bind", "biology", "bird", "birth", "bitter", "black",
    "blade", "blame", "blanket", "blast", "bleak", "bless", "blind", "blood",
    "blossom", "blouse", "blue", "blur", "blush", "board", "boat", "body",
    "boil", "bomb", "bone", "bonus", "book", "boost", "border", "boring",
    "borrow", "boss", "bottom", "bounce", "box", "boy", "bracket", "brain",
    "brand", "brass", "brave", "bread", "breeze", "brick", "bridge", "brief",
    "bright", "bring", "brisk", "broccoli", "broken", "bronze", "broom", "brother",
    "brown", "brush", "bubble", "buddy", "budget", "buffalo", "build", "bulb",
    "bulk", "bullet", "bundle", "bunker", "burden", "burger", "burst", "bus"
};

// HD Wallet structure
typedef struct {
    uint8_t seed[32];           // Master seed
    uint8_t chain_code[32];     // For key derivation
    PCKeypair master_key;       // Master keypair
    uint32_t account_index;     // Current account
    uint32_t address_index;     // Current address index
} PCHDWallet;

// Transaction history entry
typedef struct {
    uint8_t txid[32];
    uint8_t from[32];
    uint8_t to[32];
    double amount;
    uint64_t timestamp;
    int incoming;
} PCTxHistory;

// Full wallet with history
typedef struct {
    PCHDWallet hd;
    PCTxHistory* history;
    size_t history_count;
    size_t history_capacity;
    char** address_book_names;
    uint8_t (*address_book_keys)[32];
    size_t address_book_count;
} PCWalletFull;

// Generate mnemonic from entropy
int pc_mnemonic_generate(char* mnemonic, size_t len, int words) {
    if (words != 12 && words != 24) return -1;
    if (len < (size_t)(words * 12)) return -1;
    if (!mnemonic) return -1;
    
    // Ensure sodium is initialized
    if (sodium_init() < 0) {
        // Already initialized is fine (-1 means already done)
    }
    
    int entropy_bytes = (words == 12) ? 16 : 32;
    uint8_t entropy[32];
    randombytes_buf(entropy, entropy_bytes);
    
    size_t pos = 0;
    mnemonic[0] = '\0';
    
    for (int i = 0; i < words && pos < len - 20; i++) {
        int idx = entropy[i % entropy_bytes] % 256;
        const char* word = WORDLIST[idx];
        if (!word) continue;
        
        if (i > 0 && pos < len - 1) {
            mnemonic[pos++] = ' ';
        }
        
        size_t wlen = strlen(word);
        if (pos + wlen >= len) break;
        
        memcpy(mnemonic + pos, word, wlen);
        pos += wlen;
    }
    mnemonic[pos] = '\0';
    
    return 0;
}

// Convert mnemonic to seed
int pc_mnemonic_to_seed(const char* mnemonic, const char* passphrase, uint8_t* seed_out) {
    // Simple PBKDF2-like derivation (simplified for POC)
    // In production, use proper PBKDF2 with 2048 iterations
    
    char combined[512];
    snprintf(combined, sizeof(combined), "%s%s", mnemonic, passphrase ? passphrase : "");
    
    // Hash it multiple times
    uint8_t hash[32];
    crypto_generichash(hash, 32, (uint8_t*)combined, strlen(combined), NULL, 0);
    
    for (int i = 0; i < 100; i++) {
        crypto_generichash(hash, 32, hash, 32, NULL, 0);
    }
    
    memcpy(seed_out, hash, 32);
    return 0;
}

// Validate mnemonic
int pc_mnemonic_validate(const char* mnemonic) {
    if (!mnemonic || strlen(mnemonic) < 10) return 0;
    
    // Count words
    int words = 1;
    for (const char* p = mnemonic; *p; p++) {
        if (*p == ' ') words++;
    }
    
    return (words == 12 || words == 24) ? 1 : 0;
}

// Initialize HD wallet from mnemonic
int pc_hdwallet_from_mnemonic(PCHDWallet* wallet, const char* mnemonic, const char* passphrase) {
    memset(wallet, 0, sizeof(PCHDWallet));
    
    // Generate seed from mnemonic
    if (pc_mnemonic_to_seed(mnemonic, passphrase, wallet->seed) != 0) {
        return -1;
    }
    
    // Generate chain code
    uint8_t seed_extended[64];
    crypto_generichash(seed_extended, 64, wallet->seed, 32, 
                       (uint8_t*)"PhysicsCoin seed", 16);
    
    memcpy(wallet->chain_code, seed_extended + 32, 32);
    
    // Create master keypair from seed
    unsigned char pk[32], sk[64];
    crypto_sign_seed_keypair(pk, sk, wallet->seed);
    
    memcpy(wallet->master_key.public_key, pk, 32);
    memcpy(wallet->master_key.secret_key, sk, 64);
    
    wallet->account_index = 0;
    wallet->address_index = 0;
    
    return 0;
}

// Derive child keypair at index
int pc_hdwallet_derive(PCHDWallet* wallet, uint32_t index, PCKeypair* out) {
    // Simplified child key derivation
    uint8_t data[36];
    memcpy(data, wallet->chain_code, 32);
    memcpy(data + 32, &index, 4);
    
    uint8_t child_seed[32];
    crypto_generichash(child_seed, 32, data, 36, wallet->seed, 32);
    
    unsigned char pk[32], sk[64];
    crypto_sign_seed_keypair(pk, sk, child_seed);
    
    memcpy(out->public_key, pk, 32);
    memcpy(out->secret_key, sk, 64);
    
    return 0;
}

// Get new address
int pc_hdwallet_new_address(PCHDWallet* wallet, PCKeypair* out) {
    int result = pc_hdwallet_derive(wallet, wallet->address_index, out);
    if (result == 0) {
        wallet->address_index++;
    }
    return result;
}

// Save wallet to file
int pc_hdwallet_save(PCHDWallet* wallet, const char* filename, const char* password) {
    FILE* f = fopen(filename, "wb");
    if (!f) return -1;
    
    // Simple XOR encryption with password hash
    uint8_t key[32];
    crypto_generichash(key, 32, (uint8_t*)password, strlen(password), NULL, 0);
    
    uint8_t buffer[sizeof(PCHDWallet)];
    memcpy(buffer, wallet, sizeof(PCHDWallet));
    
    // XOR encrypt
    for (size_t i = 0; i < sizeof(buffer); i++) {
        buffer[i] ^= key[i % 32];
    }
    
    // Write magic + data
    uint32_t magic = 0x50435744;  // "PCWD"
    fwrite(&magic, 4, 1, f);
    fwrite(buffer, sizeof(buffer), 1, f);
    fclose(f);
    
    return 0;
}

// Load wallet from file
int pc_hdwallet_load(PCHDWallet* wallet, const char* filename, const char* password) {
    FILE* f = fopen(filename, "rb");
    if (!f) return -1;
    
    // Read magic
    uint32_t magic;
    if (fread(&magic, 4, 1, f) != 1 || magic != 0x50435744) {
        fclose(f);
        return -1;
    }
    
    // Read encrypted data
    uint8_t buffer[sizeof(PCHDWallet)];
    if (fread(buffer, sizeof(buffer), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    
    // XOR decrypt
    uint8_t key[32];
    crypto_generichash(key, 32, (uint8_t*)password, strlen(password), NULL, 0);
    
    for (size_t i = 0; i < sizeof(buffer); i++) {
        buffer[i] ^= key[i % 32];
    }
    
    memcpy(wallet, buffer, sizeof(PCHDWallet));
    return 0;
}

// Print wallet info
void pc_hdwallet_print(PCHDWallet* wallet) {
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    HD WALLET                                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Master Address: ");
    for (int i = 0; i < 8; i++) printf("%02x", wallet->master_key.public_key[i]);
    printf("...\n");
    
    printf("Addresses Generated: %u\n", wallet->address_index);
    printf("Account: %u\n\n", wallet->account_index);
}

// Export mnemonic reminder
void pc_hdwallet_backup_reminder(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  ⚠️  BACKUP YOUR MNEMONIC PHRASE!                             ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  • Write down your 12/24 words on paper                      ║\n");
    printf("║  • Store in a secure location (safe, vault)                  ║\n");
    printf("║  • NEVER share with anyone                                   ║\n");
    printf("║  • NEVER store digitally (photo, cloud, etc.)                ║\n");
    printf("║  • This is the ONLY way to recover your wallet               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
}
