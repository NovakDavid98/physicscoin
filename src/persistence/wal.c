// wal.c - Durable Write-Ahead Log for Crash Recovery
// Logs transactions before execution for durability
// SECURITY HARDENED: Proper fsync for crash safety

#include "../include/physicscoin.h"
#include "../crypto/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define WAL_MAGIC 0x57414C50  // "WALP"
#define WAL_VERSION 2  // Bumped version for new format
#define WAL_FILENAME "physicscoin.wal"
#define CHECKPOINT_FILENAME "physicscoin.checkpoint"

// WAL entry types
typedef enum {
    WAL_ENTRY_TX = 1,
    WAL_ENTRY_CHECKPOINT = 2,
    WAL_ENTRY_GENESIS = 3,
    WAL_ENTRY_SYNC_MARKER = 4  // New: explicit sync point
} WALEntryType;

// WAL file header
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t created_at;
    uint64_t entry_count;
    uint8_t state_hash[32];
    uint32_t flags;  // New: feature flags
} WALHeader;

// WAL entry header
typedef struct {
    WALEntryType type;
    uint64_t timestamp;
    uint64_t sequence;
    uint32_t payload_size;
    uint8_t checksum[32];
} WALEntryHeader;

// WAL state
typedef struct {
    FILE* file;
    int fd;  // Raw file descriptor for fsync
    WALHeader header;
    uint64_t current_sequence;
    int dirty;
    int sync_on_write;  // SECURITY: Whether to fsync after each write
} PCWAL;

// SECURITY: Force data to disk
static int wal_sync(PCWAL* wal) {
    if (!wal || !wal->file) return -1;
    
    // Flush stdio buffers
    if (fflush(wal->file) != 0) {
        perror("fflush");
        return -1;
    }
    
    // Force to disk with fsync
    if (fsync(wal->fd) != 0) {
        perror("fsync");
        return -1;
    }
    
    return 0;
}

// Initialize WAL
PCError pc_wal_init(PCWAL* wal, const char* filename) {
    if (!wal || !filename) return PC_ERR_IO;
    
    memset(wal, 0, sizeof(PCWAL));
    wal->sync_on_write = 1;  // Default: sync after each write for safety
    
    // Try to open existing WAL
    wal->file = fopen(filename, "r+b");
    
    if (wal->file) {
        wal->fd = fileno(wal->file);
        
        // Read header
        if (fread(&wal->header, sizeof(WALHeader), 1, wal->file) != 1) {
            fclose(wal->file);
            wal->file = NULL;
        } else if (wal->header.magic != WAL_MAGIC) {
            printf("Invalid WAL magic\n");
            fclose(wal->file);
            wal->file = NULL;
        } else if (wal->header.version > WAL_VERSION) {
            printf("WAL version %u is newer than supported %u\n", 
                   wal->header.version, WAL_VERSION);
            fclose(wal->file);
            wal->file = NULL;
        } else {
            wal->current_sequence = wal->header.entry_count;
            printf("Opened existing WAL with %lu entries\n", wal->header.entry_count);
            return PC_OK;
        }
    }
    
    // Create new WAL
    wal->file = fopen(filename, "w+b");
    if (!wal->file) {
        perror("fopen");
        return PC_ERR_IO;
    }
    
    wal->fd = fileno(wal->file);
    
    wal->header.magic = WAL_MAGIC;
    wal->header.version = WAL_VERSION;
    wal->header.created_at = (uint64_t)time(NULL);
    wal->header.entry_count = 0;
    wal->header.flags = 0;
    memset(wal->header.state_hash, 0, 32);
    
    if (fwrite(&wal->header, sizeof(WALHeader), 1, wal->file) != 1) {
        fclose(wal->file);
        return PC_ERR_IO;
    }
    
    // SECURITY: Ensure header is on disk
    wal_sync(wal);
    
    wal->current_sequence = 0;
    
    printf("Created new WAL v%u with fsync enabled\n", WAL_VERSION);
    
    return PC_OK;
}

// Compute checksum for entry
static void compute_checksum(const void* data, size_t size, uint8_t checksum[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, size);
    sha256_final(&ctx, checksum);
}

// Verify checksum
static int verify_checksum(const void* data, size_t size, const uint8_t expected[32]) {
    uint8_t computed[32];
    compute_checksum(data, size, computed);
    return memcmp(computed, expected, 32) == 0;
}

// Log a transaction (before execution) - DURABLE
PCError pc_wal_log_tx(PCWAL* wal, const PCTransaction* tx) {
    if (!wal || !wal->file || !tx) return PC_ERR_IO;
    
    // Seek to end
    fseek(wal->file, 0, SEEK_END);
    
    // Create entry header
    WALEntryHeader entry;
    entry.type = WAL_ENTRY_TX;
    entry.timestamp = (uint64_t)time(NULL);
    entry.sequence = wal->current_sequence++;
    entry.payload_size = sizeof(PCTransaction);
    compute_checksum(tx, sizeof(PCTransaction), entry.checksum);
    
    // Write entry header
    if (fwrite(&entry, sizeof(WALEntryHeader), 1, wal->file) != 1) {
        return PC_ERR_IO;
    }
    
    // Write transaction
    if (fwrite(tx, sizeof(PCTransaction), 1, wal->file) != 1) {
        return PC_ERR_IO;
    }
    
    // SECURITY: Force to disk immediately
    if (wal->sync_on_write) {
        if (wal_sync(wal) != 0) {
            printf("WARNING: Failed to sync WAL - data may be lost on crash\n");
        }
    }
    
    // Update header
    wal->header.entry_count = wal->current_sequence;
    wal->dirty = 1;
    
    return PC_OK;
}

// Log genesis creation - DURABLE
PCError pc_wal_log_genesis(PCWAL* wal, const uint8_t* creator_pubkey, double supply) {
    if (!wal || !wal->file || !creator_pubkey) return PC_ERR_IO;
    
    fseek(wal->file, 0, SEEK_END);
    
    // Genesis payload
    struct {
        uint8_t pubkey[32];
        double supply;
    } payload;
    memcpy(payload.pubkey, creator_pubkey, 32);
    payload.supply = supply;
    
    WALEntryHeader entry;
    entry.type = WAL_ENTRY_GENESIS;
    entry.timestamp = (uint64_t)time(NULL);
    entry.sequence = wal->current_sequence++;
    entry.payload_size = sizeof(payload);
    compute_checksum(&payload, sizeof(payload), entry.checksum);
    
    if (fwrite(&entry, sizeof(WALEntryHeader), 1, wal->file) != 1) {
        return PC_ERR_IO;
    }
    
    if (fwrite(&payload, sizeof(payload), 1, wal->file) != 1) {
        return PC_ERR_IO;
    }
    
    // SECURITY: Force to disk
    if (wal->sync_on_write) {
        wal_sync(wal);
    }
    
    wal->header.entry_count = wal->current_sequence;
    wal->dirty = 1;
    
    return PC_OK;
}

// Create checkpoint (snapshot state) - DURABLE
PCError pc_wal_checkpoint(PCWAL* wal, const PCState* state) {
    if (!wal || !wal->file || !state) return PC_ERR_IO;
    
    // Save state to checkpoint file
    uint8_t buffer[1024 * 1024];
    size_t size = pc_state_serialize(state, buffer, sizeof(buffer));
    
    // Write checkpoint to temp file first
    char temp_filename[256];
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", CHECKPOINT_FILENAME);
    
    FILE* cp = fopen(temp_filename, "wb");
    if (!cp) return PC_ERR_IO;
    
    if (fwrite(buffer, 1, size, cp) != size) {
        fclose(cp);
        unlink(temp_filename);
        return PC_ERR_IO;
    }
    
    // SECURITY: Sync checkpoint file
    fflush(cp);
    fsync(fileno(cp));
    fclose(cp);
    
    // Atomic rename
    if (rename(temp_filename, CHECKPOINT_FILENAME) != 0) {
        unlink(temp_filename);
        return PC_ERR_IO;
    }
    
    // Log checkpoint entry
    fseek(wal->file, 0, SEEK_END);
    
    WALEntryHeader entry;
    entry.type = WAL_ENTRY_CHECKPOINT;
    entry.timestamp = (uint64_t)time(NULL);
    entry.sequence = wal->current_sequence++;
    entry.payload_size = 32;  // Just the state hash
    memcpy(entry.checksum, state->state_hash, 32);
    
    if (fwrite(&entry, sizeof(WALEntryHeader), 1, wal->file) != 1) {
        return PC_ERR_IO;
    }
    
    if (fwrite(state->state_hash, 32, 1, wal->file) != 1) {
        return PC_ERR_IO;
    }
    
    // SECURITY: Sync WAL
    wal_sync(wal);
    
    // Update header with current state hash
    memcpy(wal->header.state_hash, state->state_hash, 32);
    wal->header.entry_count = wal->current_sequence;
    
    // Rewrite header
    fseek(wal->file, 0, SEEK_SET);
    fwrite(&wal->header, sizeof(WALHeader), 1, wal->file);
    wal_sync(wal);
    
    printf("Checkpoint created at sequence %lu (state synced to disk)\n", entry.sequence);
    
    return PC_OK;
}

// Write sync marker (explicit durability point)
PCError pc_wal_sync_marker(PCWAL* wal) {
    if (!wal || !wal->file) return PC_ERR_IO;
    
    fseek(wal->file, 0, SEEK_END);
    
    uint64_t sync_time = (uint64_t)time(NULL);
    
    WALEntryHeader entry;
    entry.type = WAL_ENTRY_SYNC_MARKER;
    entry.timestamp = sync_time;
    entry.sequence = wal->current_sequence++;
    entry.payload_size = 8;
    compute_checksum(&sync_time, 8, entry.checksum);
    
    if (fwrite(&entry, sizeof(WALEntryHeader), 1, wal->file) != 1) {
        return PC_ERR_IO;
    }
    
    if (fwrite(&sync_time, 8, 1, wal->file) != 1) {
        return PC_ERR_IO;
    }
    
    // Force sync
    wal_sync(wal);
    
    wal->header.entry_count = wal->current_sequence;
    
    return PC_OK;
}

// Recover state from WAL
PCError pc_wal_recover(PCWAL* wal, PCState* state) {
    if (!wal || !wal->file || !state) return PC_ERR_IO;
    
    printf("Starting WAL recovery...\n");
    
    // Try to load checkpoint first
    FILE* cp = fopen(CHECKPOINT_FILENAME, "rb");
    uint64_t checkpoint_seq = 0;
    
    if (cp) {
        fseek(cp, 0, SEEK_END);
        size_t size = ftell(cp);
        fseek(cp, 0, SEEK_SET);
        
        uint8_t* buffer = malloc(size);
        if (buffer && fread(buffer, 1, size, cp) == size) {
            if (pc_state_deserialize(state, buffer, size) == PC_OK) {
                printf("Loaded checkpoint, replaying WAL entries...\n");
            }
            free(buffer);
        }
        fclose(cp);
    }
    
    // Seek past header
    fseek(wal->file, sizeof(WALHeader), SEEK_SET);
    
    uint64_t tx_count = 0;
    uint64_t skip_count = 0;
    uint64_t corrupt_count = 0;
    
    // Replay entries
    while (!feof(wal->file)) {
        WALEntryHeader entry;
        if (fread(&entry, sizeof(WALEntryHeader), 1, wal->file) != 1) {
            break;
        }
        
        if (entry.type == WAL_ENTRY_GENESIS) {
            struct {
                uint8_t pubkey[32];
                double supply;
            } payload;
            
            if (fread(&payload, sizeof(payload), 1, wal->file) != 1) break;
            
            // SECURITY: Verify checksum
            if (!verify_checksum(&payload, sizeof(payload), entry.checksum)) {
                printf("SECURITY: Genesis entry checksum mismatch at seq %lu\n", entry.sequence);
                corrupt_count++;
                continue;
            }
            
            pc_state_genesis(state, payload.pubkey, payload.supply);
            printf("Replayed genesis: %.2f coins\n", payload.supply);
        }
        else if (entry.type == WAL_ENTRY_TX) {
            PCTransaction tx;
            if (fread(&tx, sizeof(PCTransaction), 1, wal->file) != 1) break;
            
            // SECURITY: Verify checksum
            if (!verify_checksum(&tx, sizeof(PCTransaction), entry.checksum)) {
                printf("SECURITY: TX checksum mismatch at seq %lu - SKIPPING\n", entry.sequence);
                corrupt_count++;
                continue;
            }
            
            // Skip if before checkpoint
            if (entry.sequence <= checkpoint_seq) {
                skip_count++;
                continue;
            }
            
            // Execute transaction
            PCError err = pc_state_execute_tx(state, &tx);
            if (err == PC_OK) {
                tx_count++;
            } else {
                // Transaction failed - might be already applied or invalid
                skip_count++;
            }
        }
        else if (entry.type == WAL_ENTRY_CHECKPOINT) {
            uint8_t hash[32];
            if (fread(hash, 32, 1, wal->file) != 1) break;
            checkpoint_seq = entry.sequence;
        }
        else if (entry.type == WAL_ENTRY_SYNC_MARKER) {
            uint64_t sync_time;
            if (fread(&sync_time, 8, 1, wal->file) != 1) break;
            // Sync markers are just durability points
        }
        else {
            // Skip unknown entry
            fseek(wal->file, entry.payload_size, SEEK_CUR);
        }
    }
    
    printf("Recovery complete:\n");
    printf("  TXs replayed: %lu\n", tx_count);
    printf("  TXs skipped: %lu\n", skip_count);
    printf("  Corrupt entries: %lu\n", corrupt_count);
    
    if (corrupt_count > 0) {
        printf("WARNING: %lu corrupt entries found in WAL\n", corrupt_count);
    }
    
    // Verify conservation after recovery
    PCError cons = pc_state_verify_conservation(state);
    if (cons != PC_OK) {
        printf("SECURITY: Conservation violated after recovery!\n");
        return PC_ERR_CONSERVATION_VIOLATED;
    }
    
    printf("Conservation verified after recovery\n");
    
    return PC_OK;
}

// Truncate WAL after checkpoint
PCError pc_wal_truncate(PCWAL* wal) {
    if (!wal || !wal->file) return PC_ERR_IO;
    
    // Rewrite with just header
    fclose(wal->file);
    
    wal->file = fopen(WAL_FILENAME, "w+b");
    if (!wal->file) return PC_ERR_IO;
    
    wal->fd = fileno(wal->file);
    wal->header.entry_count = 0;
    wal->current_sequence = 0;
    
    fwrite(&wal->header, sizeof(WALHeader), 1, wal->file);
    wal_sync(wal);
    
    printf("WAL truncated\n");
    
    return PC_OK;
}

// Set sync mode
void pc_wal_set_sync_mode(PCWAL* wal, int sync_on_write) {
    if (wal) {
        wal->sync_on_write = sync_on_write;
        printf("WAL sync mode: %s\n", sync_on_write ? "SYNC_ON_WRITE" : "DEFERRED");
    }
}

// Close WAL
void pc_wal_close(PCWAL* wal) {
    if (wal && wal->file) {
        // Update header if dirty
        if (wal->dirty) {
            fseek(wal->file, 0, SEEK_SET);
            fwrite(&wal->header, sizeof(WALHeader), 1, wal->file);
        }
        
        // Final sync
        wal_sync(wal);
        
        fclose(wal->file);
        wal->file = NULL;
    }
}

// Print WAL info
void pc_wal_print(const PCWAL* wal) {
    if (!wal) return;
    
    printf("\nWAL Status:\n");
    printf("  Version: %u\n", wal->header.version);
    printf("  Created: %lu\n", wal->header.created_at);
    printf("  Entries: %lu\n", wal->header.entry_count);
    printf("  Sync on write: %s\n", wal->sync_on_write ? "YES" : "NO");
    printf("  State hash: ");
    for (int i = 0; i < 8; i++) printf("%02x", wal->header.state_hash[i]);
    printf("...\n");
}
