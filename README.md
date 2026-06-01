# HoneyCrypt: A Portable C & GTK-3 Linux Desktop Vault

HoneyCrypt is a lightweight, high-performance GUI application written in pure ANSI C that implements the **Distribution-Transforming Encryption (DTE)** paradigm (also known as Honey Cryptography) using the native **GTK3 SDK**. 

Traditional symmetric encryption systems (like AES-GCM or AES-CBC with HMAC) secure private records by ensuring confidentiality, but they fail explicitly when decryption is attempted with incorrect passcodes. This explicit feedback allows adversaries to run rapid, automated offline brute-force attacks. HoneyCrypt counters this by mapping incorrect passcodes to highly plausible, syntactically correct decoy plaintexts—leaving attackers with no mathematical means of validating whether they found the true key.

---

## 📋 System Prerequisites

To build and run the GTK-3 GUI C desktop application locally, you must install the standard C development toolchain (`gcc`, `make`) and the GTK-3 developer header libraries.

### For Debian / Ubuntu Systems:
```bash
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev pkg-config
```

### For Fedora / CentOS / RHEL Systems:
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install gtk3-devel pkgconf-pkg-config
```

### For Arch Linux Systems:
```bash
sudo pacman -S base-devel gtk3 pkgconf
```

---

## 🛠️ Compilation & Build Workflows

We provide a streamlined, warning-free `Makefile` optimized with standard `-O2` native compiler flags.

To build the executable, open a terminal in the project's root folder and run:
```bash
make build
```

This will run the GCC compiler and produce a native standalone binary named `honeycrypt`:
```bash
gcc -Wall -Wextra -O2 -o honeycrypt honeycrypt.c `pkg-config --cflags --libs gtk+-3.0` -lm
```

To clean up build artifacts and delete the binary, run:
```bash
make clean
```

---

## 🚀 How to Use the Program

To start the application, execute the compiled bin file from your terminal:
```bash
./honeycrypt
```

### Step-by-Step Security Walkthrough

#### 1. Encrypting & Securing your Secret
1. **Choose a Message Schema**: Under **1. Protect the Vault**, pick a syntax schema template from the dropdown (e.g. *Credit Card Numbers*, *Crypto Mnemonic Seeds*, *Target GPS Coordinates*, *Medical Diagnostics Log*, or *Cryptographic Master Keys*).
2. **Input your Private Record**: Edit/paste your true sensitive record in the text area (e.g., your real vault keys or coordinates).
3. **Set the Passcode key**: Set your numeric or alphanumeric master PIN/password (e.g., `2938`).
4. **Trigger Generation**: Click **Encrypt & Mount Decoy Space**. 
   - The application constructs a 21-node active layout grid.
   - It hashes your PIN using `dte_hash()` to place your true message in a deterministic slot.
   - It populates the remaining 20 slots with realistic fakes that pass verification checks (like the Luhn formula for credit cards or valid BIP39 vocabularies).

#### 2. Manual Decryption Challenge
In section **2. Secure Manual Recovery Challenge**, test different PIN entries:
- **Correct Key**: Type your correct PIN (e.g., `2938`) and click **Verify Recovery**. The vault will return your genuine plaintext under validation success.
- **Incorrect Key**: Type any random wrong PIN (e.g., `9999`) and click **Verify Recovery**. Rather than erroring out, HoneyCrypt deterministic maps your wrong passcode to a plausible decoy in the storage grid. No mathematical exceptions are raised.

#### 3. Simulating an Offline Brute-Force Sweep
At the bottom of the interface, you can test both security modes against an automated brute-force cracking script:
1. **Select Standard AES Mode**: Click **Initiate Offline Brute Force**. Watch the Progress Console. Since traditional AES throws integrity exception faults for every wrong guess, the cracker script instantly isolates the single working key.
2. **Select Honey Decoy Mode**: Click the attack simulator again. Watch as *every single* candidate tried returns successfully with a plausible-looking output. When the sweep finishes, the attacker has collected 30 different, perfectly structured files and is completely blind to which one represents the true record.

---

## 📄 Academic Research Documents
We have drafted a complete academic paper describing our contributions and mathematical proofs for this implementation:
- **`honeycrypt_paper.tex`**: A fully formatted LaTeX document styled according to the *IEEE Transactions on Information Forensics and Security* template.
- **`honeycrypt_paper.txt`**: A plaintext mirror of the paper, perfect for readers on systems without a scientific PDF viewer.

---

## ⚖️ License
This project is licensed under the Apache 2.0 License - see the `SPDX-License-Identifier` header in the source file for details.
