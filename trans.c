// Secure bank transaction processing system with Hashing and Authentication.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#define MAX_ACCOUNTS 100
#define DATA_FILE "credit.dat"
#define PIN_FILE "pins.dat"

// Limits for PIN entry
#define MIN_PIN 0001
#define MAX_PIN 9999

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
int promptUnsignedInRange(const char *prompt, unsigned int min, unsigned int max, unsigned int *value);
int promptDouble(const char *prompt, double *value);
void logTransaction(const char *action, const char *details);

// UI helpers
void printScreenHeader(const char *title);
void printMessageBox(const char *label, const char *message);
void waitForEnter(void);
int readLine(char *buffer, size_t size);

// --- UI FUNCTIONS ---

void printScreenHeader(const char *title)
{
    printf("\n+----------------------------------------------------------+\n");
    printf("|                SECURE BANKING SOFTWARE (v2.0)            |\n");
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

// --- FILE INITIALIZATION ---

int ensureFileInitialized(FILE *fPtr)
{
    long expectedSize = (long)(MAX_ACCOUNTS * sizeof(struct clientData));
    struct clientData blankClient = {0, "", "", 0.0};
    long currentSize;

    if (fseek(fPtr, 0L, SEEK_END) != 0) return 0;
    currentSize = ftell(fPtr);
    
    if (currentSize >= expectedSize) {
        rewind(fPtr);
        return 1;
    }

    while (currentSize < expectedSize) {
        if (fwrite(&blankClient, sizeof(struct clientData), 1, fPtr) != 1) return 0;
        currentSize += (long)sizeof(struct clientData);
    }
    fflush(fPtr);
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

    fseek(pinPtr, 0L, SEEK_END);
    currentSize = ftell(pinPtr);

    while (currentSize < expectedSize) {
        fwrite(&blankHash, sizeof(unsigned int), 1, pinPtr);
        currentSize += (long)sizeof(unsigned int);
    }

    fclose(pinPtr);
    return 1;
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
    fseek(pinPtr, offset, SEEK_SET);
    
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
    fseek(pinPtr, offset, SEEK_SET);
    
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

    // Read stored hash
    if (!readPinHash(accountNum, &storedHash)) {
        puts("Error: Could not access security database.");
        return 0;
    }

    // If hash is 0, no PIN is set (insecure account)
    if (storedHash == 0) {
        puts("Notice: No PIN set for this account. Access granted.");
        return 1; 
    }

    while (attempts < 3) {
        printf("Enter PIN for Account %u: ", accountNum);
        if (scanf("%u", &inputPin) != 1) {
            while(getchar() != '\n'); // flush buffer
            puts("Invalid input format.");
            attempts++;
            continue;
        }
        while(getchar() != '\n'); // flush buffer after scanf

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
        printf("\nSet new PIN (%u - %u): ", MIN_PIN, MAX_PIN);
        if (!readLine(buffer, sizeof(buffer))) return 0;
        pin1 = (unsigned int)strtoul(buffer, NULL, 10);

        if (pin1 < MIN_PIN || pin1 > MAX_PIN) {
            printf("PIN must be between %u and %u digits.\n", MIN_PIN, MAX_PIN);
            continue;
        }

        // Security Rule: Avoid same username and password
        if (pin1 == accountNum) {
            puts("Security Policy: PIN cannot be the same as the Account Number.");
            continue;
        }

        printf("Confirm PIN: ");
        if (!readLine(buffer, sizeof(buffer))) return 0;
        pin2 = (unsigned int)strtoul(buffer, NULL, 10);

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
    if (errno != 0 || end == input || *end != '\0') {
        puts("Invalid amount.");
        return 0;
    }
    *value = temp;
    return 1;
}

void logTransaction(const char *action, const char *details)
{
    FILE *logFile = fopen("transactions.log", "a");
    time_t now = time(NULL);
    struct tm *timeInfo = localtime(&now);
    char timestamp[32];

    if (logFile) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeInfo);
        fprintf(logFile, "[%s] %s: %s\n", timestamp, action, details);
        fclose(logFile);
    }
}

// --- MAIN FEATURES ---

void newRecord(FILE *fPtr)
{
    printScreenHeader("ADD NEW ACCOUNT");
    unsigned int accountNum, pinHash;
    char details[160];
    struct clientData client = {0, "", "", 0.0};

    if (!promptUnsignedInRange("Enter new account number ( 1 - 100 ): ", 1, MAX_ACCOUNTS, &accountNum)) return;

    if (!readRecord(fPtr, accountNum, &client)) return;
    
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

    // Force PIN setup
    puts("\n--- SETUP SECURITY PIN ---");
    if (!promptForNewPin(accountNum, &pinHash)) return;

    client.acctNum = accountNum;
    
    if (writeRecord(fPtr, accountNum, &client)) {
        writePinHash(accountNum, pinHash); // Store the Hash
        printMessageBox("SUCCESS", "Account created and PIN hashed.");
        snprintf(details, sizeof(details), "Account %u created", accountNum);
        logTransaction("CREATE", details);
    } else {
        puts("Failed to create account.");
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

    if (!promptUnsignedInRange("Enter account to update: ", 1, MAX_ACCOUNTS, &account)) return;
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
    if (!promptDouble("Enter charge (+) or payment (-): ", &transaction)) return;

    if (client.balance + transaction < 0.0) {
        puts("Transaction rejected: insufficient balance.");
        return;
    }

    client.balance += transaction;
    if (writeRecord(fPtr, account, &client)) {
        printf("New Balance: %.2f\n", client.balance);
        snprintf(details, sizeof(details), "Acct %u updated by %.2f", account, transaction);
        logTransaction("UPDATE", details);
    }
    waitForEnter();
}

void deleteRecord(FILE *fPtr)
{
    printScreenHeader("DELETE ACCOUNT");
    unsigned int accountNum;
    struct clientData client = {0};
    struct clientData blankClient = {0};

    if (!promptUnsignedInRange("Enter account number: ", 1, MAX_ACCOUNTS, &accountNum)) return;
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
        writePinHash(accountNum, 0); // Clear hash
        printMessageBox("SUCCESS", "Account deleted.");
        logTransaction("DELETE", "Account deleted");
    }
    waitForEnter();
}

void transferFunds(FILE *fPtr)
{
    printScreenHeader("TRANSFER FUNDS");
    unsigned int fromAccount, toAccount;
    double amount;
    struct clientData fromClient = {0}, toClient = {0};

    if (!promptUnsignedInRange("Transfer FROM account: ", 1, MAX_ACCOUNTS, &fromAccount)) return;
    if (!readRecord(fPtr, fromAccount, &fromClient) || fromClient.acctNum == 0) {
        puts("Source account not found.");
        waitForEnter();
        return;
    }

    if (!promptUnsignedInRange("Transfer TO account: ", 1, MAX_ACCOUNTS, &toAccount)) return;
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

    fromClient.balance -= amount;
    toClient.balance += amount;

    if (writeRecord(fPtr, fromAccount, &fromClient) && writeRecord(fPtr, toAccount, &toClient)) {
        printMessageBox("SUCCESS", "Transfer complete.");
        char logMsg[100];
        snprintf(logMsg, sizeof(logMsg), "%.2f from %u to %u", amount, fromAccount, toAccount);
        logTransaction("TRANSFER", logMsg);
    }
    waitForEnter();
}

void changePin(FILE *fPtr)
{
    printScreenHeader("CHANGE PIN");
    unsigned int accountNum, newHash;
    struct clientData client = {0};

    if (!promptUnsignedInRange("Enter account number: ", 1, MAX_ACCOUNTS, &accountNum)) return;
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
        writePinHash(accountNum, newHash);
        printMessageBox("SUCCESS", "PIN changed successfully.");
        logTransaction("PIN_CHANGE", "User changed PIN");
    }
    waitForEnter();
}

void textFile(FILE *readPtr)
{
    FILE *writePtr;
    struct clientData client = {0};
    unsigned int account;

    if ((writePtr = fopen("accounts.txt", "w")) == NULL) {
        puts("File error.");
        return;
    }

    fprintf(writePtr, "%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
    for (account = 1; account <= MAX_ACCOUNTS; ++account) {
        if (readRecord(readPtr, account, &client) && client.acctNum == account) {
            fprintf(writePtr, "%-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName, client.balance);
        }
    }
    fclose(writePtr);
    puts("Exported to accounts.txt");
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
                     "|           [8] Exit                                       |\n"
                     "+----------------------------------------------------------+\n"
                     "Enter choice: ");

        if (!readLine(menuChoice, sizeof(menuChoice))) return 8;

        int c = atoi(menuChoice);
        if (c >= 1 && c <= 8) return c;
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
        if ((cfPtr = fopen(DATA_FILE, "wb+")) == NULL) {
            puts("File could not be opened.");
            return 1;
        }
    }

    if (!ensureFileInitialized(cfPtr) || !ensurePinFileInitialized()) {
        puts("Initialization failed.");
        return 1;
    }

    while ((choice = enterChoice()) != 8) {
        switch (choice) {
            case 1: textFile(cfPtr); break;
            case 2: updateRecord(cfPtr); break;
            case 3: newRecord(cfPtr); break;
            case 4: deleteRecord(cfPtr); break;
            case 5: listRecords(cfPtr); break;
            case 6: transferFunds(cfPtr); break;
            case 7: changePin(cfPtr); break;
            default: puts("Error"); break;
        }
    }

    fclose(cfPtr);
    return 0;
}