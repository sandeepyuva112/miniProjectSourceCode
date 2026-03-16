// Secure bank transaction processing system with Hashing and Authentication.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

#define MAX_ACCOUNTS 100u
#define DATA_FILE "credit.dat"
#define PIN_FILE "pins.dat"
#define LOG_FILE "transactions.log"
#define ACCOUNTS_EXPORT_FILE "accounts.txt"

// PIN policy (industry-style): exactly 4 digits, disallow 0000
#define PIN_LENGTH 4
#define PIN_MIN_VALUE 1u
#define PIN_MAX_VALUE 9999u

// Basic safety limit to prevent accidental huge amounts via input mistakes
#define MAX_ABS_AMOUNT 1000000000.0

#define STATEMENT_MAX_LINES 20
#define LOG_LINE_MAX 512

// clientData structure definition
struct clientData
{
    unsigned int acctNum; // account number
    char lastName[15];    // account last name
    char firstName[10];   // account first name
    double balance;       // account balance
};

// prototypes
unsigned int enterChoice(void);
void textFile(FILE *readPtr);
void updateRecord(FILE *fPtr);
void newRecord(FILE *fPtr);
void deleteRecord(FILE *fPtr);
void listRecords(FILE *fPtr);
void transferFunds(FILE *fPtr);
void changePin(FILE *fPtr);
void viewStatement(FILE *fPtr);

// File and logic helpers
int ensureFileInitialized(FILE *fPtr);
int ensurePinFileInitialized(void);
int readPinHash(unsigned int accountNum, unsigned int *pinHash);
int writePinHash(unsigned int accountNum, unsigned int pinHash);
unsigned int hashPin(unsigned int accountNum, unsigned int rawPin);
int authenticateUser(unsigned int accountNum);
int promptForNewPin(unsigned int accountNum, unsigned int *newHash);

int readRecord(FILE *fPtr, unsigned int accountNum, struct clientData *client);
int writeRecord(FILE *fPtr, unsigned int accountNum, const struct clientData *client);
int writeTwoRecordsAtomic(
    FILE *fPtr,
    unsigned int accountA, const struct clientData *oldA, const struct clientData *newA,
    unsigned int accountB, const struct clientData *oldB, const struct clientData *newB);
int promptUnsignedInRange(const char *prompt, unsigned int min, unsigned int max, unsigned int *value);
int promptDouble(const char *prompt, double *value);
void logTransaction(const char *action, const char *details);

// UI helpers
void printScreenHeader(const char *title);
void printMessageBox(const char *label, const char *message);
void waitForEnter(void);
int readLine(char *buffer, size_t size);
void printSystemError(const char *context);
int parsePin(const char *input, unsigned int *pinValue);
int lineMatchesAccount(const char *line, unsigned int accountNum);

// --- UI FUNCTIONS ---

void printScreenHeader(const char *title)
{
    printf("\n+----------------------------------------------------------+\n");
    printf("|                SECURE BANKING SOFTWARE (v2.1)            |\n");
    printf("+----------------------------------------------------------+\n");
    printf("| Screen: %-49s|\n", title);
    printf("+----------------------------------------------------------+\n");
}

void printMessageBox(const char *label, const char *message)
{
    printf("\n[%s] %s\n", label, message);
}

void waitForEnter(void)
{
    char temp[8];
    printf("\nPress Enter to continue...");
    (void)fgets(temp, sizeof(temp), stdin);
}

int readLine(char *buffer, size_t size)
{
    if (fgets(buffer, (int)size, stdin) == NULL)
        return 0;
    buffer[strcspn(buffer, "\n")] = '\0';
    return 1;
}

void printSystemError(const char *context)
{
    if (context == NULL) context = "Operation failed";

    if (errno != 0) {
        fprintf(stderr, "Error: %s: %s\n", context, strerror(errno));
        return;
    }

    fprintf(stderr, "Error: %s\n", context);
}

int parsePin(const char *input, unsigned int *pinValue)
{
    size_t len;
    unsigned int i;
    char *end = NULL;
    unsigned long parsed;

    if (input == NULL || pinValue == NULL) return 0;

    len = strlen(input);
    if (len != PIN_LENGTH) return 0;

    for (i = 0; i < PIN_LENGTH; ++i) {
        if (!isdigit((unsigned char)input[i])) return 0;
    }

    errno = 0;
    parsed = strtoul(input, &end, 10);
    if (errno != 0 || end == input || *end != '\0' || parsed < PIN_MIN_VALUE || parsed > PIN_MAX_VALUE) return 0;

    *pinValue = (unsigned int)parsed;
    return 1;
}

// --- FILE INITIALIZATION ---

int ensureFileInitialized(FILE *fPtr)
{
    long expectedSize = (long)(MAX_ACCOUNTS * sizeof(struct clientData));
    struct clientData blankClient = {0, "", "", 0.0};
    long currentSize;

    if (fseek(fPtr, 0L, SEEK_END) != 0) return 0;
    currentSize = ftell(fPtr);
    if (currentSize < 0) return 0;
    
    if (currentSize >= expectedSize) {
        rewind(fPtr);
        return 1;
    }

    while (currentSize < expectedSize) {
        if (fwrite(&blankClient, sizeof(struct clientData), 1, fPtr) != 1) return 0;
        currentSize += (long)sizeof(struct clientData);
    }
    if (fflush(fPtr) != 0) return 0;
    rewind(fPtr);
    return 1;
}

int ensurePinFileInitialized(void)
{
    FILE *pinPtr;
    unsigned int blankHash = 0;
    long expectedSize = (long)(MAX_ACCOUNTS * sizeof(unsigned int));
    long currentSize;

    pinPtr = fopen(PIN_FILE, "rb+");
    if (pinPtr == NULL) {
        pinPtr = fopen(PIN_FILE, "wb+");
        if (pinPtr == NULL) return 0;
    }

    if (fseek(pinPtr, 0L, SEEK_END) != 0) {
        fclose(pinPtr);
        return 0;
    }
    currentSize = ftell(pinPtr);
    if (currentSize < 0) {
        fclose(pinPtr);
        return 0;
    }

    while (currentSize < expectedSize) {
        if (fwrite(&blankHash, sizeof(unsigned int), 1, pinPtr) != 1) {
            fclose(pinPtr);
            return 0;
        }
        currentSize += (long)sizeof(unsigned int);
    }

    return fclose(pinPtr) == 0;
}

// --- CORE LOGIC ---

int readRecord(FILE *fPtr, unsigned int accountNum, struct clientData *client)
{
    long offset;
    if (accountNum < 1 || accountNum > MAX_ACCOUNTS) return 0;
    offset = (long)(accountNum - 1) * (long)sizeof(struct clientData);
    if (fseek(fPtr, offset, SEEK_SET) != 0) return 0;
    return fread(client, sizeof(struct clientData), 1, fPtr) == 1;
}

int writeRecord(FILE *fPtr, unsigned int accountNum, const struct clientData *client)
{
    long offset;
    if (accountNum < 1 || accountNum > MAX_ACCOUNTS) return 0;
    offset = (long)(accountNum - 1) * (long)sizeof(struct clientData);
    if (fseek(fPtr, offset, SEEK_SET) != 0) return 0;
    if (fwrite(client, sizeof(struct clientData), 1, fPtr) != 1) return 0;
    fflush(fPtr);
    return 1;
}

int writeTwoRecordsAtomic(
    FILE *fPtr,
    unsigned int accountA, const struct clientData *oldA, const struct clientData *newA,
    unsigned int accountB, const struct clientData *oldB, const struct clientData *newB)
{
    if (!writeRecord(fPtr, accountA, newA)) return 0;

    if (writeRecord(fPtr, accountB, newB)) return 1;

    int savedErrno = errno;
    (void)writeRecord(fPtr, accountB, oldB);
    (void)writeRecord(fPtr, accountA, oldA);
    errno = savedErrno;
    return 0;
}

// --- SECURITY & HASHING FUNCTIONS ---

// Simple custom hashing algorithm
// Uses the Account Number as a SALT to ensure the same PIN 
// produces different hashes for different users.
unsigned int hashPin(unsigned int accountNum, unsigned int rawPin)
{
    unsigned long hash = 5381;
    // Mix in the Account Number (Salt)
    hash = ((hash << 5) + hash) + accountNum; 
    // Mix in the PIN
    hash = ((hash << 5) + hash) + rawPin;     
    
    // Return unsigned int result
    return (unsigned int)(hash & 0xFFFFFFFF);
}

int readPinHash(unsigned int accountNum, unsigned int *pinHash)
{
    FILE *pinPtr;
    long offset;

    if (accountNum < 1 || accountNum > MAX_ACCOUNTS) return 0;

    pinPtr = fopen(PIN_FILE, "rb");
    if (pinPtr == NULL) return 0;

    offset = (long)(accountNum - 1) * (long)sizeof(unsigned int);
    if (fseek(pinPtr, offset, SEEK_SET) != 0) {
        fclose(pinPtr);
        return 0;
    }
    
    if (fread(pinHash, sizeof(unsigned int), 1, pinPtr) != 1) {
        fclose(pinPtr);
        return 0;
    }
    fclose(pinPtr);
    return 1;
}

int writePinHash(unsigned int accountNum, unsigned int pinHash)
{
    FILE *pinPtr;
    long offset;

    if (accountNum < 1 || accountNum > MAX_ACCOUNTS) return 0;

    pinPtr = fopen(PIN_FILE, "rb+");
    if (pinPtr == NULL) return 0;

    offset = (long)(accountNum - 1) * (long)sizeof(unsigned int);
    if (fseek(pinPtr, offset, SEEK_SET) != 0) {
        fclose(pinPtr);
        return 0;
    }
    
    if (fwrite(&pinHash, sizeof(unsigned int), 1, pinPtr) != 1) {
        fclose(pinPtr);
        return 0;
    }
    fclose(pinPtr);
    return 1;
}

int authenticateUser(unsigned int accountNum)
{
    unsigned int inputPin;
    unsigned int storedHash;
    unsigned int computedHash;
    int attempts = 0;
    char buffer[64];

    // Read stored hash
    if (!readPinHash(accountNum, &storedHash)) {
        puts("Error: Could not access security database.");
        return 0;
    }

    // If hash is 0, no PIN is set yet; require setting it before continuing.
    if (storedHash == 0) {
        unsigned int newHash;
        char details[96];

        printMessageBox("SECURITY NOTICE", "No PIN is set for this account. You must set one now.");
        if (!promptForNewPin(accountNum, &newHash)) {
            puts("PIN setup cancelled.");
            return 0;
        }
        if (!writePinHash(accountNum, newHash)) {
            printSystemError("Failed to store PIN");
            return 0;
        }
        snprintf(details, sizeof(details), "acct=%u pin_set=1", accountNum);
        logTransaction("PIN_SET", details);
        return 1;
    }

    while (attempts < 3) {
        printf("Enter %d-digit PIN for Account %u: ", PIN_LENGTH, accountNum);
        if (!readLine(buffer, sizeof(buffer))) return 0;

        if (!parsePin(buffer, &inputPin)) {
            puts("Invalid PIN format. Use exactly 4 digits (0001-9999).");
            attempts++;
            continue;
        }

        // Hash the input and compare
        computedHash = hashPin(accountNum, inputPin);

        if (computedHash == storedHash) {
            printf(">> Identity Verified.\n");
            return 1;
        } else {
            printf(">> Incorrect PIN. (%d/3 attempts)\n", attempts + 1);
            attempts++;
        }
    }

    printMessageBox("SECURITY ALERT", "Too many failed attempts. Transaction blocked.");
    logTransaction("AUTH_FAIL", "Multiple failed PIN attempts");
    return 0;
}

int promptForNewPin(unsigned int accountNum, unsigned int *newHash)
{
    unsigned int pin1, pin2;
    char buffer[64];

    while(1) {
        printf("\nSet new %d-digit PIN (%04u - %04u): ", PIN_LENGTH, PIN_MIN_VALUE, PIN_MAX_VALUE);
        if (!readLine(buffer, sizeof(buffer))) return 0;
        if (!parsePin(buffer, &pin1)) {
            puts("PIN must be exactly 4 digits (0001-9999).");
            continue;
        }

        // Security Rule: Avoid same username and password
        if (pin1 == accountNum) {
            puts("Security Policy: PIN cannot be the same as the Account Number.");
            continue;
        }

        printf("Confirm PIN: ");
        if (!readLine(buffer, sizeof(buffer))) return 0;
        if (!parsePin(buffer, &pin2)) {
            puts("PIN confirmation must be exactly 4 digits (0001-9999).");
            continue;
        }

        if (pin1 == pin2) {
            *newHash = hashPin(accountNum, pin1);
            return 1;
        } else {
            puts("PINs do not match. Try again.");
        }
    }
}

// --- UTILS ---

int promptUnsignedInRange(const char *prompt, unsigned int min, unsigned int max, unsigned int *value)
{
    char input[64];
    char *end = NULL;
    unsigned long parsed;

    printf("%s", prompt);
    if (!readLine(input, sizeof(input))) return 0;

    errno = 0;
    parsed = strtoul(input, &end, 10);
    if (errno != 0 || end == input || *end != '\0' || parsed < min || parsed > max) {
        puts("Invalid number or out of range.");
        return 0;
    }

    *value = (unsigned int)parsed;
    return 1;
}

int promptDouble(const char *prompt, double *value)
{
    char input[64];
    char *end = NULL;
    double temp;

    printf("%s", prompt);
    if (!readLine(input, sizeof(input))) return 0;

    errno = 0;
    temp = strtod(input, &end);
    if (errno != 0 || end == input || *end != '\0' || !isfinite(temp) || fabs(temp) > MAX_ABS_AMOUNT) {
        puts("Invalid amount.");
        return 0;
    }
    *value = temp;
    return 1;
}

void logTransaction(const char *action, const char *details)
{
    FILE *logFile = fopen(LOG_FILE, "a");
    time_t now = time(NULL);
    struct tm *timeInfo = localtime(&now);
    char timestamp[32] = "unknown";

    if (logFile == NULL) return;
    if (timeInfo != NULL) {
        (void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeInfo);
    }

    if (action == NULL) action = "UNKNOWN";
    if (details == NULL) details = "";

    fprintf(logFile, "[%s] %s: %s\n", timestamp, action, details);
    fclose(logFile);
}

int lineMatchesAccount(const char *line, unsigned int accountNum)
{
    char needle[32];

    if (line == NULL) return 0;

    snprintf(needle, sizeof(needle), "acct=%u", accountNum);
    if (strstr(line, needle) != NULL) return 1;

    snprintf(needle, sizeof(needle), "from=%u", accountNum);
    if (strstr(line, needle) != NULL) return 1;

    snprintf(needle, sizeof(needle), "to=%u", accountNum);
    if (strstr(line, needle) != NULL) return 1;

    // Legacy patterns (older log messages)
    snprintf(needle, sizeof(needle), "from %u", accountNum);
    if (strstr(line, needle) != NULL) return 1;

    snprintf(needle, sizeof(needle), "to %u", accountNum);
    if (strstr(line, needle) != NULL) return 1;

    snprintf(needle, sizeof(needle), "Account %u", accountNum);
    const char *pos = strstr(line, needle);
    if (pos != NULL) {
        char next = pos[strlen(needle)];
        if (next == '\0' || !isdigit((unsigned char)next)) return 1;
    }

    snprintf(needle, sizeof(needle), "Acct %u", accountNum);
    pos = strstr(line, needle);
    if (pos != NULL) {
        char next = pos[strlen(needle)];
        if (next == '\0' || !isdigit((unsigned char)next)) return 1;
    }

    return 0;
}

// --- MAIN FEATURES ---

void newRecord(FILE *fPtr)
{
    printScreenHeader("ADD NEW ACCOUNT");
    unsigned int accountNum, pinHash;
    char details[160];
    struct clientData client = {0, "", "", 0.0};

    char prompt[64];
    snprintf(prompt, sizeof(prompt), "Enter new account number ( 1 - %u ): ", MAX_ACCOUNTS);
    if (!promptUnsignedInRange(prompt, 1, MAX_ACCOUNTS, &accountNum)) {
        waitForEnter();
        return;
    }

    if (!readRecord(fPtr, accountNum, &client)) {
        printSystemError("Failed to read account database");
        waitForEnter();
        return;
    }
    
    if (client.acctNum == accountNum) {
        printf("Account #%u already exists.\n", client.acctNum);
        waitForEnter();
        return;
    }

    char input[128];
    char extra[2];
    printf("Enter lastname, firstname, balance\n? ");
    if (!readLine(input, sizeof(input)) ||
        sscanf(input, "%14s %9s %lf %1s", client.lastName, client.firstName, &client.balance, extra) != 3) {
        puts("Invalid customer details.");
        waitForEnter();
        return;
    }
    if (!isfinite(client.balance) || client.balance < 0.0 || client.balance > MAX_ABS_AMOUNT) {
        puts("Invalid opening balance.");
        waitForEnter();
        return;
    }

    // Force PIN setup
    puts("\n--- SETUP SECURITY PIN ---");
    if (!promptForNewPin(accountNum, &pinHash)) {
        waitForEnter();
        return;
    }

    client.acctNum = accountNum;
    
    if (writeRecord(fPtr, accountNum, &client)) {
        if (!writePinHash(accountNum, pinHash)) {
            printSystemError("Failed to store PIN");
            struct clientData blankClient = {0};
            if (!writeRecord(fPtr, accountNum, &blankClient)) {
                printSystemError("Rollback failed (account record)");
            }
            snprintf(details, sizeof(details), "acct=%u reason=pin_store_failed", accountNum);
            logTransaction("CREATE_FAIL", details);
            printMessageBox("ERROR", "Account creation rolled back. Please try again.");
            waitForEnter();
            return;
        }
        printMessageBox("SUCCESS", "Account created.");
        snprintf(details, sizeof(details), "acct=%u last=%s first=%s opening_balance=%.2f", accountNum, client.lastName, client.firstName, client.balance);
        logTransaction("CREATE", details);
    } else {
        printSystemError("Failed to create account");
    }
    waitForEnter();
}

void updateRecord(FILE *fPtr)
{
    printScreenHeader("UPDATE ACCOUNT");
    unsigned int account;
    double transaction;
    char details[160];
    struct clientData client = {0, "", "", 0.0};

    if (!promptUnsignedInRange("Enter account to update: ", 1, MAX_ACCOUNTS, &account)) {
        waitForEnter();
        return;
    }
    if (!readRecord(fPtr, account, &client) || client.acctNum != account) {
        puts("Account not found.");
        waitForEnter();
        return;
    }

    // SECURITY CHECK
    if (!authenticateUser(account)) {
        waitForEnter();
        return;
    }

    printf("Current Balance: %.2f\n", client.balance);
    if (!promptDouble("Enter charge (+) or payment (-): ", &transaction)) {
        waitForEnter();
        return;
    }

    double oldBalance = client.balance;
    double newBalance = client.balance + transaction;
    if (!isfinite(newBalance) || newBalance < 0.0) {
        puts("Transaction rejected: insufficient balance.");
        waitForEnter();
        return;
    }

    client.balance = newBalance;
    if (writeRecord(fPtr, account, &client)) {
        printf("New Balance: %.2f\n", client.balance);
        snprintf(details, sizeof(details), "acct=%u delta=%.2f before=%.2f after=%.2f", account, transaction, oldBalance, client.balance);
        logTransaction("UPDATE", details);
    } else {
        printSystemError("Failed to write account database");
    }
    waitForEnter();
}

void deleteRecord(FILE *fPtr)
{
    printScreenHeader("DELETE ACCOUNT");
    unsigned int accountNum;
    struct clientData client = {0};
    struct clientData blankClient = {0};

    if (!promptUnsignedInRange("Enter account number: ", 1, MAX_ACCOUNTS, &accountNum)) {
        waitForEnter();
        return;
    }
    if (!readRecord(fPtr, accountNum, &client) || client.acctNum != accountNum) {
        puts("Account not found.");
        waitForEnter();
        return;
    }

    // SECURITY CHECK
    if (!authenticateUser(accountNum)) {
        waitForEnter();
        return;
    }

    if (writeRecord(fPtr, accountNum, &blankClient)) {
        if (!writePinHash(accountNum, 0)) {
            printSystemError("Failed to clear PIN");
        }
        printMessageBox("SUCCESS", "Account deleted.");
        char details[160];
        snprintf(details, sizeof(details), "acct=%u last=%s first=%s", accountNum, client.lastName, client.firstName);
        logTransaction("DELETE", details);
    } else {
        printSystemError("Failed to delete account");
    }
    waitForEnter();
}

void transferFunds(FILE *fPtr)
{
    printScreenHeader("TRANSFER FUNDS");
    unsigned int fromAccount, toAccount;
    double amount;
    struct clientData fromClient = {0}, toClient = {0};

    if (!promptUnsignedInRange("Transfer FROM account: ", 1, MAX_ACCOUNTS, &fromAccount)) {
        waitForEnter();
        return;
    }
    if (!readRecord(fPtr, fromAccount, &fromClient) || fromClient.acctNum == 0) {
        puts("Source account not found.");
        waitForEnter();
        return;
    }

    if (!promptUnsignedInRange("Transfer TO account: ", 1, MAX_ACCOUNTS, &toAccount)) {
        waitForEnter();
        return;
    }
    if (!readRecord(fPtr, toAccount, &toClient) || toClient.acctNum == 0) {
        puts("Destination account not found.");
        waitForEnter();
        return;
    }

    if (fromAccount == toAccount) {
        puts("Cannot transfer to self.");
        waitForEnter();
        return;
    }

    if (!promptDouble("Amount: ", &amount) || amount <= 0) {
        puts("Invalid amount.");
        waitForEnter();
        return;
    }

    if (fromClient.balance < amount) {
        puts("Insufficient funds.");
        waitForEnter();
        return;
    }

    // SECURITY CHECK (Only Sender needs to auth)
    printf("\nAuthenticating Sender (Account %u)...\n", fromAccount);
    if (!authenticateUser(fromAccount)) {
        waitForEnter();
        return;
    }

    struct clientData fromOriginal = fromClient;
    struct clientData toOriginal = toClient;
    double fromBefore = fromClient.balance;
    double toBefore = toClient.balance;

    fromClient.balance -= amount;
    toClient.balance += amount;
    if (!isfinite(fromClient.balance) || !isfinite(toClient.balance) || fromClient.balance < 0.0) {
        puts("Transfer rejected due to invalid resulting balance.");
        waitForEnter();
        return;
    }

    if (writeTwoRecordsAtomic(
            fPtr,
            fromAccount, &fromOriginal, &fromClient,
            toAccount, &toOriginal, &toClient)) {
        printMessageBox("SUCCESS", "Transfer complete.");
        char logMsg[100];
        snprintf(logMsg, sizeof(logMsg), "from=%u to=%u amount=%.2f from_before=%.2f from_after=%.2f to_before=%.2f to_after=%.2f",
                 fromAccount, toAccount, amount, fromBefore, fromClient.balance, toBefore, toClient.balance);
        logTransaction("TRANSFER", logMsg);
    } else {
        printSystemError("Transfer failed while writing account database");
    }
    waitForEnter();
}

void changePin(FILE *fPtr)
{
    printScreenHeader("CHANGE PIN");
    unsigned int accountNum, newHash;
    struct clientData client = {0};

    if (!promptUnsignedInRange("Enter account number: ", 1, MAX_ACCOUNTS, &accountNum)) {
        waitForEnter();
        return;
    }
    if (!readRecord(fPtr, accountNum, &client) || client.acctNum == 0) {
        puts("Account not found.");
        waitForEnter();
        return;
    }

    // Must know OLD pin to set NEW pin
    puts("Please verify current credentials:");
    if (!authenticateUser(accountNum)) {
        waitForEnter();
        return;
    }

    if (promptForNewPin(accountNum, &newHash)) {
        if (!writePinHash(accountNum, newHash)) {
            printSystemError("Failed to store PIN");
        } else {
            printMessageBox("SUCCESS", "PIN changed successfully.");
            char details[96];
            snprintf(details, sizeof(details), "acct=%u pin_changed=1", accountNum);
            logTransaction("PIN_CHANGE", details);
        }
    }
    waitForEnter();
}

void textFile(FILE *readPtr)
{
    FILE *writePtr;
    struct clientData client = {0};
    unsigned int account;

    if ((writePtr = fopen(ACCOUNTS_EXPORT_FILE, "w")) == NULL) {
        printSystemError("Could not create export file");
        waitForEnter();
        return;
    }

    fprintf(writePtr, "%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
    for (account = 1; account <= MAX_ACCOUNTS; ++account) {
        if (readRecord(readPtr, account, &client) && client.acctNum == account) {
            fprintf(writePtr, "%-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName, client.balance);
        }
    }
    if (fclose(writePtr) != 0) {
        printSystemError("Could not close export file");
    } else {
        printf("Exported to %s\n", ACCOUNTS_EXPORT_FILE);
    }
    waitForEnter();
}

void listRecords(FILE *fPtr)
{
    struct clientData client = {0};
    unsigned int account;
    printScreenHeader("LIST ACCOUNTS");
    printf("%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
    for (account = 1; account <= MAX_ACCOUNTS; ++account) {
        if (readRecord(fPtr, account, &client) && client.acctNum == account) {
            printf("%-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName, client.balance);
        }
    }
    waitForEnter();
}

void viewStatement(FILE *fPtr)
{
    printScreenHeader("ACCOUNT STATEMENT");
    unsigned int accountNum;
    struct clientData client = {0};

    if (!promptUnsignedInRange("Enter account number: ", 1, MAX_ACCOUNTS, &accountNum)) {
        waitForEnter();
        return;
    }

    if (!readRecord(fPtr, accountNum, &client) || client.acctNum == 0) {
        puts("Account not found.");
        waitForEnter();
        return;
    }

    if (!authenticateUser(accountNum)) {
        waitForEnter();
        return;
    }

    FILE *logPtr = fopen(LOG_FILE, "r");
    if (logPtr == NULL) {
        printSystemError("Could not open transaction log");
        waitForEnter();
        return;
    }

    char ring[STATEMENT_MAX_LINES][LOG_LINE_MAX] = {{0}};
    unsigned int matchCount = 0;
    char line[LOG_LINE_MAX];

    while (fgets(line, sizeof(line), logPtr) != NULL) {
        if (!lineMatchesAccount(line, accountNum)) continue;

        unsigned int slot = matchCount % STATEMENT_MAX_LINES;
        size_t i = 0;
        while (i + 1 < LOG_LINE_MAX && line[i] != '\0') {
            ring[slot][i] = line[i];
            ++i;
        }
        ring[slot][i] = '\0';
        matchCount++;
    }

    fclose(logPtr);

    printf("\nAccount: %u  Name: %s %s\n", client.acctNum, client.firstName, client.lastName);
    printf("Current Balance: %.2f\n", client.balance);

    if (matchCount == 0) {
        puts("\nNo transactions found for this account.");
        waitForEnter();
        return;
    }

    unsigned int linesToShow = matchCount < STATEMENT_MAX_LINES ? matchCount : STATEMENT_MAX_LINES;
    unsigned int startIndex = (matchCount >= STATEMENT_MAX_LINES) ? (matchCount % STATEMENT_MAX_LINES) : 0;

    puts("\n--- Last Transactions ---");
    for (unsigned int i = 0; i < linesToShow; ++i) {
        const char *entry = ring[(startIndex + i) % STATEMENT_MAX_LINES];
        fputs(entry, stdout);
        if (entry[0] != '\0' && entry[strlen(entry) - 1] != '\n') putchar('\n');
    }

    waitForEnter();
}

unsigned int enterChoice(void)
{
    char menuChoice[32];
    
    while (1) {
        printScreenHeader("MAIN MENU");
        printf("%s", 
                     "|           [1] Export Accounts                            |\n"
                     "|           [2] Update Account (Auth Required)             |\n"
                     "|           [3] Add New Account (Set PIN)                  |\n"
                     "|           [4] Delete Account (Auth Required)             |\n"
                     "|           [5] List Active Accounts                       |\n"
                     "|           [6] Transfer Funds (Auth Required)             |\n"
                     "|           [7] Change PIN                                 |\n"
                     "|           [8] View Statement (Auth Required)             |\n"
                     "|           [9] Exit                                       |\n"
                     "+----------------------------------------------------------+\n"
                     "Enter choice: ");

        if (!readLine(menuChoice, sizeof(menuChoice))) return 9;

        errno = 0;
        char *end = NULL;
        unsigned long parsed = strtoul(menuChoice, &end, 10);
        if (errno == 0 && end != menuChoice && *end == '\0' && parsed >= 1 && parsed <= 9) {
            return (unsigned int)parsed;
        }
        puts("Invalid choice.");
    }
}

int main(int argc, char *argv[])
{
    FILE *cfPtr;
    unsigned int choice;
    (void)argc; (void)argv;
    srand((unsigned int)time(NULL));

    if ((cfPtr = fopen(DATA_FILE, "rb+")) == NULL) {
        cfPtr = fopen(DATA_FILE, "wb+");
        if (cfPtr == NULL) {
            printSystemError("Could not open data file");
            return 1;
        }
    }

    if (!ensureFileInitialized(cfPtr)) {
        printSystemError("Data file initialization failed");
        fclose(cfPtr);
        return 1;
    }

    if (!ensurePinFileInitialized()) {
        printSystemError("PIN database initialization failed");
        fclose(cfPtr);
        return 1;
    }

    while ((choice = enterChoice()) != 9) {
        switch (choice) {
            case 1: textFile(cfPtr); break;
            case 2: updateRecord(cfPtr); break;
            case 3: newRecord(cfPtr); break;
            case 4: deleteRecord(cfPtr); break;
            case 5: listRecords(cfPtr); break;
            case 6: transferFunds(cfPtr); break;
            case 7: changePin(cfPtr); break;
            case 8: viewStatement(cfPtr); break;
            default: puts("Error"); break;
        }
    }

    fclose(cfPtr);
    return 0;
}
