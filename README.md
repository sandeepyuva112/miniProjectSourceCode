# Transaction Processing System  
### Mini Project - 24UCS271 Lab

---

## Overview
CLI-based banking/ATM-style transaction processing system with a file-backed account database, PIN authentication, and an audit log.

### Features
- Create/update/delete accounts
- Transfer funds between accounts (with rollback on partial write failure)
- PIN authentication (4-digit PIN, 3 attempts, change PIN)
- Export accounts to `accounts.txt`
- View account statement (last 20 matching log entries)
- Append-only audit log in `transactions.log`

### Build (GCC / MinGW)
`gcc -Wall -Wextra -Wpedantic -std=c11 -O2 trans.c -o trans.exe`

### Build (CMake)
`cmake --preset mingw`
`cmake --build --preset mingw`

### Run
`./trans.exe`

### Data files
- `credit.dat`: binary account database (auto-created if missing)
- `pins.dat`: binary PIN-hash database (auto-created if missing)
- `transactions.log`: audit log (auto-created if missing)
- `accounts.txt`: exported report

### Security notes
- PIN must be exactly 4 digits (`0001` to `9999`) and is required for sensitive operations.
- PIN hashing in this mini-project is for learning/demo. For real production systems, use Argon2/bcrypt/scrypt/PBKDF2 with strong salts and secure random.

## Objective
This mini project focuses on understanding, modifying, and extending a **Transaction Processing System** program. Students are expected to analyze existing code, improve it, and add new functionalities while following good coding practices.

---

## Reference Code
Review the given source code before starting the project:

- https://replit.com/@ashokb/Unit5Programs#trans.c

---

## Tasks to be Performed

1. Compile and run the given executable code.
2. Perform your own testing of the program.
3. Add a new account detail and regenerate the `accounts.txt` file.
   - Inspect the contents of the file.
   - Verify whether the output matches your expectations.
4. Identify and fix any logical errors in the program.
5. Add new functionality  
   - Example: Listing all account information.
6. Optimize the program for better performance and efficiency.

---

## Evaluation Criteria

### 1. General (25 Points)
| Activity | Points | Remarks |
|--------|--------|--------|
| Self-effort | 5 | Student's own assessment of hours spent (no copying or external references) |
| Turnaround Time | 10 | Provide three time slots; earlier demo gets more points |
| Project Demo | 10 | Demo must be done using repl.it |

---

### 2. Comprehension (15 Points)
| Activity | Points |
|--------|--------|
| Domain Knowledge | 5 |
| Added Functionality (Ideas) | 5 |
| Code Comprehension | 5 |

---

### 3. Modification (35 Points)
| Activity | Points |
|--------|--------|
| Code Improvement | 5 |
| Functional Decomposition | 10 |
| Refactoring for Memory Usage | 10 |
| Refactoring for Speed | 10 |

---

### 4. Innovation (45 Points)
| Activity | Points |
|--------|--------|
| New Features / User Stories (Faculty specified) | 5 |
| Requirement to Code Translation | 10 |
| Added Functionality (Simple - Error Handling) | 10 |
| Added Functionality (Advanced) | 20 |

---

## Total Marks
**120 Marks**  
-> Reduced to **20 Marks** for final evaluation

---

## Rules for Mini Project Implementation

- Maximum of **2 attempts** will be given for project presentation.
- If dissatisfied with the first score, a second attempt is allowed.
- The **best score** out of the two attempts will be considered final.
- Discussion and clarification among students are allowed.
- **Copying or borrowing code is strictly prohibited.**
- Violation of plagiarism rules will result in a **zero (0 / NULL) score**.

---

## Additional Reference
- http://j.mp/transactionUnit5
- https://htmlpreview.github.io/?https://github.com/kgisl/makesite/blob/master/content/blog/2021-06-23-unit5-mini-project.md.html
- https://github.com/24UCS271-MiniProject/miniProjectSourceCode/

---

**Date:** 31/01/2026  
**Time:** 06:42
