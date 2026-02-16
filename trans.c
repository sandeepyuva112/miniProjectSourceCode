// Upgraded bank transaction processing system.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#define MAX_ACCOUNTS 100
#define DATA_FILE "credit.dat"

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

int ensureFileInitialized(FILE *fPtr);
int readRecord(FILE *fPtr, unsigned int accountNum, struct clientData *client);
int writeRecord(FILE *fPtr, unsigned int accountNum, const struct clientData *client);
int promptUnsignedInRange(const char *prompt, unsigned int min, unsigned int max, unsigned int *value);
int promptDouble(const char *prompt, double *value);
void logTransaction(const char *action, const char *details);
void printScreenHeader(const char *title);
void printMessageBox(const char *label, const char *message);
void waitForEnter(void);
int readLine(char *buffer, size_t size);

void printScreenHeader(const char *title)
{
    printf("\n+----------------------------------------------------------+\n");
    printf("|                ABSOLUTE BANKING SOFTWARE                |\n");
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
    {
        return 0;
    }

    buffer[strcspn(buffer, "\n")] = '\0';
    return 1;
}


int ensureFileInitialized(FILE *fPtr)
{
    long expectedSize = (long)(MAX_ACCOUNTS * sizeof(struct clientData));
    struct clientData blankClient = {0, "", "", 0.0};
    long currentSize;

    if (fseek(fPtr, 0L, SEEK_END) != 0)
    {
        return 0;
    }

    currentSize = ftell(fPtr);
    if (currentSize < 0)
    {
        return 0;
    }

    if (currentSize >= expectedSize)
    {
        rewind(fPtr);
        return 1;
    }

    if (fseek(fPtr, 0L, SEEK_END) != 0)
    {
        return 0;
    }

    while (currentSize < expectedSize)
    {
        if (fwrite(&blankClient, sizeof(struct clientData), 1, fPtr) != 1)
        {
            return 0;
        }
        currentSize += (long)sizeof(struct clientData);
    }

    fflush(fPtr);
    rewind(fPtr);
    return 1;
}

int readRecord(FILE *fPtr, unsigned int accountNum, struct clientData *client)
{
    long offset;

    if (accountNum < 1 || accountNum > MAX_ACCOUNTS)
    {
        return 0;
    }

    offset = (long)(accountNum - 1) * (long)sizeof(struct clientData);
    if (fseek(fPtr, offset, SEEK_SET) != 0)
    {
        return 0;
    }

    return fread(client, sizeof(struct clientData), 1, fPtr) == 1;
}

int writeRecord(FILE *fPtr, unsigned int accountNum, const struct clientData *client)
{
    long offset;

    if (accountNum < 1 || accountNum > MAX_ACCOUNTS)
    {
        return 0;
    }

    offset = (long)(accountNum - 1) * (long)sizeof(struct clientData);
    if (fseek(fPtr, offset, SEEK_SET) != 0)
    {
        return 0;
    }

    if (fwrite(client, sizeof(struct clientData), 1, fPtr) != 1)
    {
        return 0;
    }

    fflush(fPtr);
    return 1;
}

int promptUnsignedInRange(const char *prompt, unsigned int min, unsigned int max, unsigned int *value)
{
    char input[64];
    char *end = NULL;
    unsigned long parsed;

    printf("%s", prompt);
    if (!readLine(input, sizeof(input)))
    {
        return 0;
    }

    errno = 0;
    parsed = strtoul(input, &end, 10);
    if (errno != 0 || end == input || *end != '\0')
    {
        puts("Invalid number.");
        return 0;
    }

    if (parsed < min || parsed > max)
    {
        puts("Value out of range.");
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
    if (!readLine(input, sizeof(input)))
    {
        return 0;
    }

    errno = 0;
    temp = strtod(input, &end);
    if (errno != 0 || end == input || *end != '\0')
    {
        puts("Invalid amount.");
        return 0;
    }

    *value = temp;
    return 1;
}

void logTransaction(const char *action, const char *details)
{
    FILE *logFile;
    time_t now;
    struct tm *timeInfo;
    char timestamp[32];

    logFile = fopen("transactions.log", "a");
    if (logFile == NULL)
    {
        return;
    }

    now = time(NULL);
    timeInfo = localtime(&now);
    if (timeInfo != NULL)
    {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeInfo);
        fprintf(logFile, "[%s] %s: %s\n", timestamp, action, details);
    }
    else
    {
        fprintf(logFile, "[time-unavailable] %s: %s\n", action, details);
    }

    fclose(logFile);
}

int main(int argc, char *argv[])
{
    FILE *cfPtr;
    unsigned int choice;

    (void)argc;

    if ((cfPtr = fopen(DATA_FILE, "rb+")) == NULL)
    {
        if ((cfPtr = fopen(DATA_FILE, "wb+")) == NULL)
        {
            printf("%s: File could not be opened.\n", argv[0]);
            return 1;
        }
    }

    if (!ensureFileInitialized(cfPtr))
    {
        puts("Unable to initialize account storage.");
        fclose(cfPtr);
        return 1;
    }

    while ((choice = enterChoice()) != 7)
    {
        switch (choice)
        {
        case 1:
            textFile(cfPtr);
            break;
        case 2:
            updateRecord(cfPtr);
            break;
        case 3:
            newRecord(cfPtr);
            break;
        case 4:
            deleteRecord(cfPtr);
            break;
        case 5:
            listRecords(cfPtr);
            break;
        case 6:
            transferFunds(cfPtr);
            break;
        default:
            puts("Incorrect choice");
            break;
        }
    }

    fclose(cfPtr);
    return 0;
}

// create formatted text file for printing
void textFile(FILE *readPtr)
{
    printScreenHeader("EXPORT ACCOUNTS");
    FILE *writePtr;
    struct clientData client = {0, "", "", 0.0};
    unsigned int recordsWritten = 0;
    unsigned int account = 1;

    if ((writePtr = fopen("accounts.txt", "w")) == NULL)
    {
        puts("File could not be opened.");
        return;
    }

    fprintf(writePtr, "%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");

    for (account = 1; account <= MAX_ACCOUNTS; ++account)
    {
        if (readRecord(readPtr, account, &client) && client.acctNum == account)
        {
            fprintf(writePtr, "%-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName,
                    client.balance);
            ++recordsWritten;
        }
    }

    fclose(writePtr);
    printf("accounts.txt generated with %u active record(s).\n", recordsWritten);
    waitForEnter();
}

// update balance in record
void updateRecord(FILE *fPtr)
{
    printScreenHeader("UPDATE ACCOUNT");
    unsigned int account;
    double transaction;
    char details[160];
    struct clientData client = {0, "", "", 0.0};

    if (!promptUnsignedInRange("Enter account to update ( 1 - 100 ): ", 1, MAX_ACCOUNTS, &account))
    {
        return;
    }

    if (!readRecord(fPtr, account, &client) || client.acctNum != account)
    {
        printf("Account #%u has no information.\n", account);
        waitForEnter();
        return;
    }

    printf("%-6u%-16s%-11s%10.2f\n\n", client.acctNum, client.lastName, client.firstName, client.balance);

    if (!promptDouble("Enter charge ( + ) or payment ( - ): ", &transaction))
    {
        return;
    }

    if (client.balance + transaction < 0.0)
    {
        puts("Transaction rejected: insufficient balance.");
        waitForEnter();
        return;
    }

    client.balance += transaction;
    if (!writeRecord(fPtr, account, &client))
    {
        puts("Failed to update account.");
        waitForEnter();
        return;
    }

    printMessageBox("SUCCESS", "Account updated successfully.");
    printf("Updated balance: %.2f\n", client.balance);
    snprintf(details, sizeof(details), "Account %u updated by %.2f (new balance %.2f)", account, transaction,
             client.balance);
    logTransaction("UPDATE", details);
    waitForEnter();
}

// delete an existing record
void deleteRecord(FILE *fPtr)
{
    printScreenHeader("DELETE ACCOUNT");
    unsigned int accountNum;
    char details[120];
    struct clientData client = {0, "", "", 0.0};
    struct clientData blankClient = {0, "", "", 0.0};

    if (!promptUnsignedInRange("Enter account number to delete ( 1 - 100 ): ", 1, MAX_ACCOUNTS, &accountNum))
    {
        return;
    }

    if (!readRecord(fPtr, accountNum, &client) || client.acctNum != accountNum)
    {
        printf("Account %u does not exist.\n", accountNum);
        waitForEnter();
        return;
    }

    if (!writeRecord(fPtr, accountNum, &blankClient))
    {
        puts("Failed to delete account.");
        waitForEnter();
        return;
    }

    printMessageBox("SUCCESS", "Account deleted successfully.");
    printf("Account %u deleted successfully.\n", accountNum);
    snprintf(details, sizeof(details), "Account %u deleted", accountNum);
    logTransaction("DELETE", details);
    waitForEnter();
}

// create and insert record
void newRecord(FILE *fPtr)
{
    printScreenHeader("ADD NEW ACCOUNT");
    unsigned int accountNum;
    char details[160];
    struct clientData client = {0, "", "", 0.0};

    if (!promptUnsignedInRange("Enter new account number ( 1 - 100 ): ", 1, MAX_ACCOUNTS, &accountNum))
    {
        return;
    }

    if (!readRecord(fPtr, accountNum, &client))
    {
        puts("Unable to access account storage.");
        return;
    }

    if (client.acctNum == accountNum)
    {
        printf("Account #%u already contains information.\n", client.acctNum);
        return;
    }

    {
        char input[128];
        char extra[2];

        printf("Enter lastname, firstname, balance\n? ");
        if (!readLine(input, sizeof(input)) ||
            sscanf(input, "%14s %9s %lf %1s", client.lastName, client.firstName, &client.balance, extra) != 3)
        {
            puts("Invalid customer details.");
            waitForEnter();
            return;
        }
    }

    if (client.balance < 0.0)
    {
        puts("Opening balance cannot be negative.");
        waitForEnter();
        return;
    }

    client.acctNum = accountNum;
    if (!writeRecord(fPtr, accountNum, &client))
    {
        puts("Failed to create account.");
        waitForEnter();
        return;
    }

    printMessageBox("SUCCESS", "Account created successfully.");
    printf("Account %u created successfully.\n", accountNum);
    snprintf(details, sizeof(details), "Account %u created for %s %s with %.2f", accountNum, client.firstName,
             client.lastName, client.balance);
    logTransaction("CREATE", details);
    waitForEnter();
}

// list all active accounts
void listRecords(FILE *fPtr)
{
    printScreenHeader("LIST ACTIVE ACCOUNTS");
    struct clientData client = {0, "", "", 0.0};
    unsigned int account;
    unsigned int count = 0;

    printf("\n%-6s%-16s%-11s%10s\n", "Acct", "Last Name", "First Name", "Balance");
    printf("------------------------------------------------\n");

    for (account = 1; account <= MAX_ACCOUNTS; ++account)
    {
        if (readRecord(fPtr, account, &client) && client.acctNum == account)
        {
            printf("%-6u%-16s%-11s%10.2f\n", client.acctNum, client.lastName, client.firstName, client.balance);
            ++count;
        }
    }

    if (count == 0)
    {
        puts("No active accounts found.");
    }
    else
    {
        printf("Total active accounts: %u\n", count);
    }
    waitForEnter();
}

// transfer funds between two accounts
void transferFunds(FILE *fPtr)
{
    printScreenHeader("TRANSFER FUNDS");
    unsigned int fromAccount;
    unsigned int toAccount;
    double amount;
    char details[180];
    struct clientData fromClient = {0, "", "", 0.0};
    struct clientData toClient = {0, "", "", 0.0};

    if (!promptUnsignedInRange("Transfer from account (1-100): ", 1, MAX_ACCOUNTS, &fromAccount))
    {
        return;
    }

    if (!promptUnsignedInRange("Transfer to account (1-100): ", 1, MAX_ACCOUNTS, &toAccount))
    {
        return;
    }

    if (fromAccount == toAccount)
    {
        puts("Source and destination accounts must be different.");
        waitForEnter();
        return;
    }

    if (!promptDouble("Transfer amount: ", &amount) || amount <= 0.0)
    {
        puts("Transfer amount must be greater than zero.");
        waitForEnter();
        return;
    }

    if (!readRecord(fPtr, fromAccount, &fromClient) || fromClient.acctNum != fromAccount)
    {
        printf("Source account %u does not exist.\n", fromAccount);
        waitForEnter();
        return;
    }

    if (!readRecord(fPtr, toAccount, &toClient) || toClient.acctNum != toAccount)
    {
        printf("Destination account %u does not exist.\n", toAccount);
        waitForEnter();
        return;
    }

    if (fromClient.balance < amount)
    {
        puts("Transfer rejected: insufficient balance.");
        waitForEnter();
        return;
    }

    fromClient.balance -= amount;
    toClient.balance += amount;

    if (!writeRecord(fPtr, fromAccount, &fromClient) || !writeRecord(fPtr, toAccount, &toClient))
    {
        puts("Transfer failed due to file write error.");
        waitForEnter();
        return;
    }

    printMessageBox("SUCCESS", "Transfer completed successfully.");
    printf("Transfer successful: %.2f from %u to %u\n", amount, fromAccount, toAccount);
    snprintf(details, sizeof(details), "%.2f transferred from %u to %u", amount, fromAccount, toAccount);
    logTransaction("TRANSFER", details);
    waitForEnter();
}

// enable user to input menu choice
unsigned int enterChoice(void)
{
    char menuChoice[32];
    unsigned int i;

    while (1)
    {
        printScreenHeader("MAIN MENU");
        printf("%s", "|  [E]/1 Export Accounts                        |\n"
                     "|  [U]/2 Update Existing Account                |\n"
                     "|  [A]/3 Add New Account                        |\n"
                     "|  [D]/4 Delete Account                         |\n"
                     "|  [L]/5 List Active Accounts                   |\n"
                     "|  [T]/6 Transfer Funds                         |\n"
                     "|  [X]/7 Exit                                   |\n"
                     "+----------------------------------------------------------+\n"
                     "Enter command: ");

        if (!readLine(menuChoice, sizeof(menuChoice)))
        {
            if (feof(stdin))
            {
                return 7;
            }
            puts("Invalid choice. Please type a valid option.");
            continue;
        }

        for (i = 0; menuChoice[i] != '\0'; ++i)
        {
            menuChoice[i] = (char)tolower((unsigned char)menuChoice[i]);
        }

        if (strcmp(menuChoice, "1") == 0 || strcmp(menuChoice, "e") == 0 || strcmp(menuChoice, "export") == 0)
        {
            return 1;
        }
        if (strcmp(menuChoice, "2") == 0 || strcmp(menuChoice, "u") == 0 || strcmp(menuChoice, "update") == 0)
        {
            return 2;
        }
        if (strcmp(menuChoice, "3") == 0 || strcmp(menuChoice, "a") == 0 || strcmp(menuChoice, "add") == 0)
        {
            return 3;
        }
        if (strcmp(menuChoice, "4") == 0 || strcmp(menuChoice, "d") == 0 || strcmp(menuChoice, "delete") == 0)
        {
            return 4;
        }
        if (strcmp(menuChoice, "5") == 0 || strcmp(menuChoice, "l") == 0 || strcmp(menuChoice, "list") == 0)
        {
            return 5;
        }
        if (strcmp(menuChoice, "6") == 0 || strcmp(menuChoice, "t") == 0 || strcmp(menuChoice, "transfer") == 0)
        {
            return 6;
        }
        if (strcmp(menuChoice, "7") == 0 || strcmp(menuChoice, "x") == 0 || strcmp(menuChoice, "exit") == 0)
        {
            return 7;
        }

        puts("Invalid choice. Use 1-7, E/U/A/D/L/T/X, or full commands.");
    }
}
