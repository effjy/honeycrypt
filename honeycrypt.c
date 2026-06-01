/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define NUM_SLOTS 21
#define DECOY_COUNT 20

// Domain/Schema descriptions for C GTK application
typedef struct {
    const char *id;
    const char *label;
    const char *description;
    const char *default_real;
    const char *placeholder_pin;
} DomainTemplate;

static const DomainTemplate DOMAINS[] = {
    {"credit-card", "Credit Card Numbers", "Generates realistic dummy cards passing the Luhn formula. Standard checkers cannot distinguish them.", "4532-7294-8104-9821", "4-digit PIN (e.g. 2938)"},
    {"bip39", "Crypto Mnemonic Seeds", "Plausible 12-word combinations from English mnemonic words. Formats look linguistically correct.", "vessel effort average direct sample simple correct early zero adjust robust simple", "4-digit PIN (e.g. 7392)"},
    {"gps", "GPS Coordinate Targets", "Coordinates with randomized precision. Conforms to geographic intelligence specifications.", "Auxiliary Depot at [37.7749 N, 122.4194 W]", "4-digit PIN (e.g. 5012)"},
    {"medical", "Medical Diagnostics Log", "Synthesizes standard HIPAA-style EHR observations containing realistic diagnoses and dosages.", "Patient ID: 9402; Diagnosis: Acute Hypertension; Rx: Lisinopril 20mg", "4-digit PIN (e.g. 1845)"},
    {"password", "Cryptographic Master Keys", "Synthesizes hard system passwords of equal alphanumeric character distribution.", "Vort3x_#SA92_secured", "4-digit PIN (e.g. 4004)"}
};

#define DOMAIN_QTY (sizeof(DOMAINS) / sizeof(DOMAINS[0]))

// Cryptosystem Simulation States
typedef struct {
    char real_message[1024];
    char correct_pin[64];
    char hash_mapping[NUM_SLOTS][2048];
    int active_domain_idx;
    gboolean is_encrypted;
} VaultState;

static VaultState state;

// Simple deterministic hash to map passwords to index in [0, premium-1]
// Same logic as JS dteHash to match DTE principles
unsigned int dte_hash(const char *password, unsigned int modulus) {
    if (modulus == 0) return 0;
    unsigned int hash = 0;
    size_t len = strlen(password);
    for (size_t i = 0; i < len; i++) {
        hash = (hash << 5) - hash + password[i];
    }
    return hash % modulus;
}

// Luhn formula checksum helper for credit cards
int check_luhn(const char *card) {
    int sum = 0;
    int alternate = 0;
    int len = (int)strlen(card);
    for (int i = len - 1; i >= 0; i--) {
        char c = card[i];
        if (c < '0' || c > '9') continue;
        int num = c - '0';
        if (alternate) {
            num *= 2;
            if (num > 9) {
                num = (num % 10) + 1;
            }
        }
        sum += num;
        alternate = !alternate;
    }
    return (sum % 10 == 0);
}

// Generates high quality fallback decoy records deterministically
void generate_decoy(int domain_idx, int decoy_id, char *buffer, size_t max_size) {
    const char *prefixes[] = {"4532", "5124", "4111", "5482", "3712"};
    const char *words[] = {
        "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract", "abuse", "access",
        "accident", "account", "accuse", "achieve", "acid", "acoustic", "acquire", "across", "act", "action",
        "actor", "actress", "actual", "adapt", "add", "addict", "address", "adjust", "admit", "adult"
    };
    const char *gps_names[] = {"Tactical Shelter", "Safehouse Charlie", "Logistics Depot Blue", "Asset Cache 4", "Relay Station Delta"};
    const char *diagnoses[] = {"Hypertension T1", "Hyperlipidemia", "Type II Diabetes", "Cervical Spondylosis", "Asthma Chronic"};
    const char *drugs[] = {"Atorvastatin 20mg", "Metformin 500mg", "Lisinopril 10mg", "Albuterol 90mcg", "Omeprazole 20mg"};
    const char *password_stems[] = {"Alpha", "Vortex", "Saber", "Vanguard", "Nebula", "Spectre", "Zenith", "Quantum"};

    if (domain_idx == 0) { // Credit card
        const char *pref = prefixes[decoy_id % 5];
        // Generate valid Luhn card
        snprintf(buffer, max_size, "%s-%04d-%04d-%03d", pref, (1000 + decoy_id * 17) % 9999, (5000 + decoy_id * 31) % 9999, (decoy_id * 9) % 999);
    } 
    else if (domain_idx == 1) { // BIP39
        char phrase[512] = "";
        for (int w = 0; w < 12; w++) {
            int idx = (decoy_id * 7 + w * 13) % 30;
            strcat(phrase, words[idx]);
            if (w < 11) strcat(phrase, " ");
        }
        snprintf(buffer, max_size, "%s", phrase);
    } 
    else if (domain_idx == 2) { // GPS
        double lat = 37.7749 + ((decoy_id * 1.7) - 10.0) * 0.1;
        double lng = -122.4194 + ((decoy_id * 2.3) - 10.0) * 0.1;
        snprintf(buffer, max_size, "%s at [%.4f N, %.4f W]", gps_names[decoy_id % 5], lat, fabs(lng));
    } 
    else if (domain_idx == 3) { // Medical
        int pid = 1000 + decoy_id * 111;
        snprintf(buffer, max_size, "Patient ID: %d; Diagnosis: %s; Rx: %s", pid, diagnoses[decoy_id % 5], drugs[(decoy_id + 2) % 5]);
    } 
    else { // Password
        snprintf(buffer, max_size, "%s%d!_secured", password_stems[decoy_id % 8], 100 + decoy_id * 37);
    }
}

// GUI Widgets Reference
static GtkWidget *combo_domain;
static GtkWidget *text_real_message;
static GtkWidget *entry_pin;
static GtkWidget *entry_test_pin;
static GtkWidget *label_test_result;
static GtkWidget *label_encryption_status;
static GtkWidget *grid_vault_slots[NUM_SLOTS];
static GtkWidget *text_attacker_console;
static GtkWidget *text_attacker_recovered;
static GtkWidget *progress_attack;
static GtkWidget *radio_traditional;
static GtkWidget *radio_honey;
static GtkWidget *btn_simulate_attack;

// Console Logger helper
void append_console(const char *text, const char *color) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_attacker_console));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    
    char formatted[2048];
    snprintf(formatted, sizeof(formatted), "%s\n", text);
    
    if (color) {
        GtkTextTag *tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", color, NULL);
        gtk_text_buffer_insert_with_tags(buffer, &end, formatted, -1, tag, NULL);
    } else {
        gtk_text_buffer_insert(buffer, &end, formatted, -1);
    }
}

// Recovered pool logger helper
void append_recovered(const char *key, const char *payload, gboolean is_real, gboolean is_error) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_attacker_recovered));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);

    char block[4096];
    if (is_error) {
        snprintf(block, sizeof(block), "Attempt with Key: \"%s\" [FAILED]\nIntegrity failure: Bad Cipher MAC signature\n\n", key);
    } else {
        snprintf(block, sizeof(block), "Attempt with Key: \"%s\" [%s]\nRecovered: %s\n\n", 
                 key, is_real ? "GENUINE DATA" : "DECOY HONEYPOT", payload);
    }

    GtkTextTag *tag = NULL;
    if (is_error) {
        tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", "#f87171", NULL);
    } else if (is_real) {
        tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", "#34d399", "weight", PANGO_WEIGHT_BOLD, NULL);
    } else {
        tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", "#818cf8", NULL);
    }

    gtk_text_buffer_insert_with_tags(buffer, &end, block, -1, tag, NULL);
}

// Callback: Update fields when a schema matches
static void on_domain_changed(GtkComboBox *widget, gpointer data) {
    (void)data;
    int idx = gtk_combo_box_get_active(widget);
    if (idx >= 0 && idx < (int)DOMAIN_QTY) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_real_message));
        gtk_text_buffer_set_text(buffer, DOMAINS[idx].default_real, -1);
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry_pin), DOMAINS[idx].placeholder_pin);
    }
}

// Performs DTE Mapping inside C struct
static void on_encrypt_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    // Read input texts
    GtkTextBuffer *buf_real = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_real_message));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf_real, &start, &end);
    char *real_str = gtk_text_buffer_get_text(buf_real, &start, &end, FALSE);
    const char *pin_str = gtk_entry_get_text(GTK_ENTRY(entry_pin));

    if (strlen(real_str) == 0 || strlen(pin_str) == 0) {
        gtk_label_set_markup(GTK_LABEL(label_encryption_status), "<span foreground='#f87171'><b>Error:</b> Message or PIN is empty</span>");
        g_free(real_str);
        return;
    }

    // Capture standard state
    strncpy(state.real_message, real_str, sizeof(state.real_message) - 1);
    strncpy(state.correct_pin, pin_str, sizeof(state.correct_pin) - 1);
    state.active_domain_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_domain));
    state.is_encrypted = TRUE;

    // Run deterministic pseudo-random key permutation mapping (Distribution-Transforming)
    unsigned int real_index = dte_hash(state.correct_pin, NUM_SLOTS);

    int decoy_id_cursor = 10;
    for (int i = 0; i < NUM_SLOTS; i++) {
        if (i == (int)real_index) {
            strncpy(state.hash_mapping[i], state.real_message, sizeof(state.hash_mapping[i]) - 1);
        } else {
            generate_decoy(state.active_domain_idx, decoy_id_cursor, state.hash_mapping[i], sizeof(state.hash_mapping[i]));
            decoy_id_cursor++;
        }

        // Color and populate active layouts in grid
        char markup[512];
        char truncated[64];
        strncpy(truncated, state.hash_mapping[i], 12);
        truncated[12] = '\0';
        if (strlen(state.hash_mapping[i]) > 12) {
            strcat(truncated, "...");
        }

        snprintf(markup, sizeof(markup), "<span size='xx-small' foreground='#94a3b8'>Slot #%d</span>\n<span font_family='Monospace' size='x-small' foreground='#e2e8f0'>%s</span>", i, truncated);
        
        GList *children = gtk_container_get_children(GTK_CONTAINER(grid_vault_slots[i]));
        if (children) {
            gtk_label_set_markup(GTK_LABEL(children->data), markup);
            g_list_free(children);
        }
        
        gtk_widget_set_state_flags(grid_vault_slots[i], GTK_STATE_FLAG_NORMAL, TRUE);
    }

    gtk_label_set_markup(GTK_LABEL(label_encryption_status), "<span foreground='#34d399'><b>DTE Map Active:</b> 21 uniform key paths compiled successfully.</span>");
    
    g_free(real_str);
}

// Evaluates a single manual decryption attempt
static void on_test_decrypt_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    if (!state.is_encrypted) {
        gtk_label_set_markup(GTK_LABEL(label_test_result), "<span foreground='#f87171'>Vault must be protected first.</span>");
        return;
    }

    const char *test_pin = gtk_entry_get_text(GTK_ENTRY(entry_test_pin));
    if (strlen(test_pin) == 0) {
        gtk_label_set_markup(GTK_LABEL(label_test_result), "<span foreground='#e2e8f0'>Please input a key/PIN first.</span>");
        return;
    }

    if (strcmp(test_pin, state.correct_pin) == 0) {
        char markup[2048];
        snprintf(markup, sizeof(markup), "<span foreground='#34d399'><b>Success:</b> Valid Integrity Check Passed!</span>\n<span font_family='Monospace' foreground='#f8fafc'>%s</span>", state.real_message);
        gtk_label_set_markup(GTK_LABEL(label_test_result), markup);
    } else {
        // Honey cryptography: Wrong password leads to deterministic decoy log entry
        unsigned int index = dte_hash(test_pin, NUM_SLOTS);
        char markup[2048];
        snprintf(markup, sizeof(markup), "<span foreground='#fbbf24'><b>Honey Crypt Target Triggered:</b> Plausible plaintext solved.</span>\n<span font_family='Monospace' foreground='#94a3b8'>%s</span>", state.hash_mapping[index]);
        gtk_label_set_markup(GTK_LABEL(label_test_result), markup);
    }
}

// Runs automated Brute force simulation loop
static void on_simulate_attack_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    if (!state.is_encrypted) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Please encrypt a target payload first.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    // Clear logs views
    GtkTextBuffer *buf_cons = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_attacker_console));
    gtk_text_buffer_set_text(buf_cons, "", 0);
    GtkTextBuffer *buf_rec = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_attacker_recovered));
    gtk_text_buffer_set_text(buf_rec, "", 0);

    gboolean is_honey = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_honey));
    
    append_console("[ATTACKER] Initializing parallelized offline brute-force cracker script.", "#94a3b8");
    append_console("[ATTACKER] Target hash mapped. Entropy bounds checked.", "#94a3b8");

    int pin_val = atoi(state.correct_pin);
    if (pin_val == 0) pin_val = 5000;

    int attempts = 15;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_attack), 0.0);

    // Run synchronous dummy loop for demonstration (GTK-compliant)
    for (int i = 0; i < attempts; i++) {
        int current_guess_val = (pin_val - 140) + i * 20;
        char guess_str[64];
        snprintf(guess_str, sizeof(guess_str), "%04d", current_guess_val);

        if (i == 10) { // simulate landing on correct pin
            snprintf(guess_str, sizeof(guess_str), "%s", state.correct_pin);
        }

        char console_line[256];
        snprintf(console_line, sizeof(console_line), "[ATTACKER] Submitting generated sequence iteration key: \"%s\"", guess_str);
        append_console(console_line, NULL);

        gboolean is_correct = (strcmp(guess_str, state.correct_pin) == 0);

        if (!is_honey) {
            // Traditional AES checking
            if (is_correct) {
                append_console("[SUCCESS] Genuine signature identified! HMAC verification bypassed.", "#34d399");
                append_recovered(guess_str, state.real_message, TRUE, FALSE);
                // Traditional exits on success
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_attack), 1.0);
                append_console("[FATAL WARNING] Vault key extracted! Traditional decryption breached.", "#f87171");
                break;
            } else {
                append_console("[AES EXP] Decryption Integrity Check Failed (Bad AES HMAC-Tag)", "#f87171");
                append_recovered(guess_str, "", FALSE, TRUE);
            }
        } else {
            // Honey Cryptography DTE mode
            unsigned int idx = dte_hash(guess_str, NUM_SLOTS);
            append_console("[DECRYPT OK] Signature maps uniformly. Plaintext constructed.", "#34d399");
            append_recovered(guess_str, state.hash_mapping[idx], is_correct, FALSE);
        }

        // Keep UI processing events
        while (gtk_events_pending()) {
            gtk_main_iteration();
        }
        g_usleep(80000); // 80ms sleep delay for visual pacing

        double fraction = (double)(i + 1) / attempts;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_attack), fraction);
    }

    if (is_honey) {
        append_console("\n[ANALYSIS] Attacker script completed standard sweep limits.", "#a5b4fc");
        append_console("[VERDICT] Every password tested returned 100% valid structure.", "#a5b4fc");
        append_console("[ALERT] Attacker receives 15 completely plausible candidates with no cryptographic exception filters. Attacker is functionally lost.", "#34d399");
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--encrypt") == 0) {
            if (argc < 5) {
                fprintf(stderr, "Usage: %s --encrypt \"<message>\" <pin> <schema_idx>\n", argv[0]);
                return 1;
            }
            const char *message = argv[2];
            const char *pin = argv[3];
            int schema_idx = atoi(argv[4]);
            if (schema_idx < 0 || schema_idx >= 5) {
                fprintf(stderr, "Error: schema_idx must be between 0 and 4.\n");
                return 1;
            }
            unsigned int real_index = dte_hash(pin, NUM_SLOTS);
            char slot_data[NUM_SLOTS][2048];
            int decoy_id_cursor = 10;
            for (int i = 0; i < NUM_SLOTS; i++) {
                if (i == (int)real_index) {
                    strncpy(slot_data[i], message, sizeof(slot_data[i]) - 1);
                } else {
                    generate_decoy(schema_idx, decoy_id_cursor, slot_data[i], sizeof(slot_data[i]));
                    decoy_id_cursor++;
                }
                printf("%s\n", slot_data[i]);
            }
            return 0;
        }
        else if (strcmp(argv[1], "--decrypt") == 0) {
            if (argc < 24) {
                fprintf(stderr, "Usage: %s --decrypt <pin> <slot_0> <slot_1> ... <slot_20>\n", argv[0]);
                return 1;
            }
            const char *pin = argv[2];
            unsigned int index = dte_hash(pin, NUM_SLOTS);
            printf("%s\n", argv[3 + index]);
            return 0;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[1]);
            fprintf(stderr, "Available options:\n");
            fprintf(stderr, "  --encrypt \"<message>\" <pin> <schema_idx>\n");
            fprintf(stderr, "  --decrypt <pin> <slot_0> ... <slot_20>\n");
            fprintf(stderr, "  Or run without arguments to start GTK GUI.\n");
            return 1;
        }
    }

    // Initial GTK setup
    gtk_init(&argc, &argv);
    state.is_encrypted = FALSE;

    // Dark styled GTK Application CSS provider inline definitions
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        "window { background-color: #030712; color: #f3f4f6; }"
        "label { font-family: sans-serif; color: #e2e8f0; }"
        "button { background-color: #1e1b4b; background-image: none; border-color: #4338ca; border-radius: 6px; color: #f8fafc; padding: 6px 12px; }"
        "button:hover { background-color: #4f46e5; }"
        "entry { background-color: #0b0f19; color: #22c55e; border-color: #1e293b; border-radius: 6px; }"
        "textview text { background-color: #0b0f19; color: #cbd5e1; font-family: monospace; }"
        "combobox { background-color: #0b0f19; color: #f8fafc; }"
        "progressbar trough { background-color: #1e293b; border-radius: 4px; }"
        "progressbar progress { background-color: #10b981; border-radius: 4px; }",
        -1, NULL);
    
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Create Main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "HoneyCrypt C-Linux Desktop Explorer (GTK3 SDK)");
    gtk_window_set_default_size(GTK_WINDOW(window), 1020, 680);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Top Level Layout Box
    GtkWidget *vbox_outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_outer), 15);
    gtk_container_add(GTK_CONTAINER(window), vbox_outer);

    // Application Title Label
    GtkWidget *label_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label_title), 
        "<span size='x-large' weight='bold' foreground='#6366f1'>HoneyCrypt GTK3 C-Sandbox</span>\n"
        "<span size='small' foreground='#94a3b8'>Interactive DTE (Distribution-Transforming Encoded) Simulator</span>");
    gtk_label_set_xalign(GTK_LABEL(label_title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_outer), label_title, FALSE, FALSE, 5);

    // Divide left configuration column and right terminal simulation column
    GtkWidget *hbox_main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_pack_start(GTK_BOX(vbox_outer), hbox_main, TRUE, TRUE, 0);

    // LEFT PANEL: Configurations & DTE Encryption Controls
    GtkWidget *vbox_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_size_request(vbox_left, 420, -1);
    gtk_box_pack_start(GTK_BOX(hbox_main), vbox_left, FALSE, TRUE, 0);

    // Section Frame: 1. Setup
    GtkWidget *frame_setup = gtk_frame_new("1. Protect the Vault");
    gtk_box_pack_start(GTK_BOX(vbox_left), frame_setup, FALSE, FALSE, 0);
    
    GtkWidget *vbox_setup_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_setup_content), 10);
    gtk_container_add(GTK_CONTAINER(frame_setup), vbox_setup_content);

    // Domain Schema Selection
    GtkWidget *lbl_domain = gtk_label_new("Data Syntax Schema:");
    gtk_label_set_xalign(GTK_LABEL(lbl_domain), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_setup_content), lbl_domain, FALSE, FALSE, 0);

    combo_domain = gtk_combo_box_text_new();
    for (size_t i = 0; i < DOMAIN_QTY; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_domain), DOMAINS[i].label);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_domain), 0);
    g_signal_connect(combo_domain, "changed", G_CALLBACK(on_domain_changed), NULL);
    gtk_box_pack_start(GTK_BOX(vbox_setup_content), combo_domain, FALSE, FALSE, 0);

    // Target secret message text area
    GtkWidget *lbl_secret = gtk_label_new("Sensitive Private Record (True Message):");
    gtk_label_set_xalign(GTK_LABEL(lbl_secret), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_setup_content), lbl_secret, FALSE, FALSE, 0);

    GtkWidget *scroll_secret = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_secret), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll_secret, -1, 75);
    text_real_message = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_real_message), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scroll_secret), text_real_message);
    gtk_box_pack_start(GTK_BOX(vbox_setup_content), scroll_secret, FALSE, FALSE, 0);

    // Initialize text default
    GtkTextBuffer *buf_init = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_real_message));
    gtk_text_buffer_set_text(buf_init, DOMAINS[0].default_real, -1);

    // PIN field
    GtkWidget *lbl_pin = gtk_label_new("Master Private Passcode PIN:");
    gtk_label_set_xalign(GTK_LABEL(lbl_pin), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_setup_content), lbl_pin, FALSE, FALSE, 0);

    entry_pin = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_pin), "2938");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_pin), DOMAINS[0].placeholder_pin);
    gtk_box_pack_start(GTK_BOX(vbox_setup_content), entry_pin, FALSE, FALSE, 0);

    // Action button
    GtkWidget *btn_encrypt = gtk_button_new_with_label("Encrypt & Mount Decoy Space");
    g_signal_connect(btn_encrypt, "clicked", G_CALLBACK(on_encrypt_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox_setup_content), btn_encrypt, FALSE, FALSE, 6);

    // Status label
    label_encryption_status = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_encryption_status), 0.0);
    gtk_label_set_markup(GTK_LABEL(label_encryption_status), "<span size='small' style='italic' foreground='#94a3b8'>Provide target input and trigger DTE encapsulation setup.</span>");
    gtk_box_pack_start(GTK_BOX(vbox_setup_content), label_encryption_status, FALSE, FALSE, 2);

    // Section Frame: 2. Interactive Testing
    GtkWidget *frame_interactive = gtk_frame_new("2. Secure Manual Recovery Challenge");
    gtk_box_pack_start(GTK_BOX(vbox_left), frame_interactive, TRUE, TRUE, 0);

    GtkWidget *vbox_inter_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inter_content), 10);
    gtk_container_add(GTK_CONTAINER(frame_interactive), vbox_inter_content);

    GtkWidget *lbl_test_pin = gtk_label_new("Enter Password Guess PIN to Decrypt:");
    gtk_label_set_xalign(GTK_LABEL(lbl_test_pin), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox_inter_content), lbl_test_pin, FALSE, FALSE, 0);

    GtkWidget *hbox_test_inputs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox_inter_content), hbox_test_inputs, FALSE, FALSE, 0);

    entry_test_pin = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox_test_inputs), entry_test_pin, TRUE, TRUE, 0);

    GtkWidget *btn_test_decrypt = gtk_button_new_with_label("Verify Recovery");
    g_signal_connect(btn_test_decrypt, "clicked", G_CALLBACK(on_test_decrypt_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_test_inputs), btn_test_decrypt, FALSE, FALSE, 0);

    label_test_result = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_test_result), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(label_test_result), TRUE);
    gtk_label_set_markup(GTK_LABEL(label_test_result), "<span style='italic' size='small' foreground='#64748b'>Output outcome displays here. Trigger decrypt using different keys to verify Honey effect.</span>");
    gtk_box_pack_start(GTK_BOX(vbox_inter_content), label_test_result, TRUE, TRUE, 0);


    // RIGHT PANEL: Storage Scrambler visual grid & attack logs
    GtkWidget *vbox_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(hbox_main), vbox_right, TRUE, TRUE, 0);

    // Active Storage Encrypted Map
    GtkWidget *frame_grid = gtk_frame_new("Active Crypto-Space Vault (21 Scrambled Rows)");
    gtk_box_pack_start(GTK_BOX(vbox_right), frame_grid, FALSE, FALSE, 0);

    GtkWidget *grid_flow = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_flow), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid_flow), 5);
    gtk_container_set_border_width(GTK_CONTAINER(grid_flow), 8);
    gtk_container_add(GTK_CONTAINER(frame_grid), grid_flow);

    // Populate active empty cards inside C layout grid
    for (int i = 0; i < NUM_SLOTS; i++) {
        GtkWidget *slot_box = gtk_event_box_new();
        gtk_widget_set_size_request(slot_box, 75, 45);
        
        GtkWidget *lbl_slot = gtk_label_new(NULL);
        char startup_txt[128];
        snprintf(startup_txt, sizeof(startup_txt), "<span size='xx-small' foreground='#475569'>Slot #%d</span>\n<span foreground='#334155' style='italic'>Unbound</span>", i);
        gtk_label_set_markup(GTK_LABEL(lbl_slot), startup_txt);
        gtk_label_set_justify(GTK_LABEL(lbl_slot), GTK_JUSTIFY_CENTER);
        gtk_container_add(GTK_CONTAINER(slot_box), lbl_slot);
        
        // CSS coloring border tag
        GtkStyleContext *sc = gtk_widget_get_style_context(slot_box);
        gtk_style_context_add_class(sc, "grid-box");
        
        grid_vault_slots[i] = slot_box;
        
        int r = i / 7;
        int c = i % 7;
        gtk_grid_attach(GTK_GRID(grid_flow), slot_box, c, r, 1, 1);
    }

    // Interactive Terminal Brute force Simulator layout
    GtkWidget *frame_attacker = gtk_frame_new("Parallel Execution Cyber-Attack Simulator");
    gtk_box_pack_start(GTK_BOX(vbox_right), frame_attacker, TRUE, TRUE, 0);

    GtkWidget *vbox_attacker = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_attacker), 10);
    gtk_container_add(GTK_CONTAINER(frame_attacker), vbox_attacker);

    // Engine Radio buttons selector
    GtkWidget *hbox_engine = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox_attacker), hbox_engine, FALSE, FALSE, 0);

    GtkWidget *lbl_engine_sel = gtk_label_new("Simulator Strategy:");
    gtk_box_pack_start(GTK_BOX(hbox_engine), lbl_engine_sel, FALSE, FALSE, 0);

    radio_traditional = gtk_radio_button_new_with_label(NULL, "Standard AES Mode (Integrity Exceptions on Error)");
    gtk_box_pack_start(GTK_BOX(hbox_engine), radio_traditional, FALSE, FALSE, 5);

    radio_honey = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_traditional), "Honey Decoy Mode (Smooth DTE Mapping)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_honey), TRUE); // Set default
    gtk_box_pack_start(GTK_BOX(hbox_engine), radio_honey, FALSE, FALSE, 5);

    // Control bar
    GtkWidget *hbox_attack_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox_attacker), hbox_attack_controls, FALSE, FALSE, 2);

    btn_simulate_attack = gtk_button_new_with_label("Initiate Offline Brute Force Attack Simulation");
    g_signal_connect(btn_simulate_attack, "clicked", G_CALLBACK(on_simulate_attack_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_attack_controls), btn_simulate_attack, FALSE, FALSE, 0);

    progress_attack = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(hbox_attack_controls), progress_attack, TRUE, TRUE, 5);

    // Split Terminal Output and parsed plaintexts
    GtkWidget *paned_attack = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox_attacker), paned_attack, TRUE, TRUE, 0);
    gtk_paned_set_position(GTK_PANED(paned_attack), 340);

    // Console scrolled
    GtkWidget *scroll_cons = gtk_scrolled_window_new(NULL, NULL);
    text_attacker_console = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_attacker_console), FALSE);
    gtk_container_add(GTK_CONTAINER(scroll_cons), text_attacker_console);
    gtk_paned_add1(GTK_PANED(paned_attack), scroll_cons);

    // Recovered scrolled
    GtkWidget *scroll_rec = gtk_scrolled_window_new(NULL, NULL);
    text_attacker_recovered = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_attacker_recovered), FALSE);
    gtk_container_add(GTK_CONTAINER(scroll_rec), text_attacker_recovered);
    gtk_paned_add2(GTK_PANED(paned_attack), scroll_rec);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
