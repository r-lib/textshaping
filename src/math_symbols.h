#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>

namespace math {

// Forward declaration from math_parser.h
enum class MathStyle;

// =============================================================================
// Symbol Tables (extern declarations - definitions in math_symbols.cpp)
// =============================================================================

// Greek letters: command -> Unicode codepoint
extern const std::unordered_map<std::string, uint32_t> greek_letters;

// Binary operators
extern const std::unordered_map<std::string, uint32_t> binary_operators;

// Relations
extern const std::unordered_map<std::string, uint32_t> relations;

// Arrows
extern const std::unordered_map<std::string, uint32_t> arrows;

// Big operators
extern const std::unordered_map<std::string, uint32_t> big_operators;

// Accents (single character)
extern const std::unordered_map<std::string, uint32_t> accents;

// Wide accents
extern const std::unordered_map<std::string, uint32_t> wide_accents;

// Function names
extern const std::unordered_set<std::string> functions;

// Delimiters
extern const std::unordered_map<std::string, uint32_t> delimiters;

// Miscellaneous symbols
extern const std::unordered_map<std::string, uint32_t> misc_symbols;

// Space commands: command -> em units (1em = 18mu in TeX)
extern const std::unordered_map<std::string, double> space_commands;

// Style commands: command -> MathStyle
extern const std::unordered_map<std::string, MathStyle> style_commands;

// Matrix environment names
extern const std::unordered_set<std::string> matrix_environments;

// Cases environment names
extern const std::unordered_set<std::string> cases_environments;

// Alignment environment names
extern const std::unordered_set<std::string> alignment_environments;

// Big delimiter commands - map to size level (1=\big, 2=\Big, 3=\bigg, 4=\Bigg)
extern const std::unordered_map<std::string, int> big_delim_commands;

// Display style commands
extern const std::unordered_set<std::string> display_style_commands;

// Fraction variants
extern const std::unordered_set<std::string> fraction_variants;

// Over/under arrow commands
extern const std::unordered_set<std::string> over_under_arrows;

// Mod commands
extern const std::unordered_set<std::string> mod_commands;

// Stack commands
extern const std::unordered_set<std::string> stack_commands;

// Phantom commands
extern const std::unordered_set<std::string> phantom_commands;

// Color commands
extern const std::unordered_set<std::string> color_commands;

// Cancel commands
extern const std::unordered_set<std::string> cancel_commands;

// Extensible arrow commands
extern const std::unordered_set<std::string> extensible_arrows;

// =============================================================================
// Classification functions
// =============================================================================

bool is_greek_letter(const std::string& cmd);
bool is_binary_operator(const std::string& cmd);
bool is_relation(const std::string& cmd);
bool is_arrow(const std::string& cmd);
bool is_big_operator(const std::string& cmd);
bool is_accent_command(const std::string& cmd);
bool is_wide_accent_command(const std::string& cmd);
bool is_function_command(const std::string& cmd);
bool is_delimiter(const std::string& cmd);
bool is_space_command(const std::string& cmd);
bool is_style_command(const std::string& cmd);
MathStyle style_command_to_style(const std::string& cmd);
bool is_matrix_environment(const std::string& name);
bool is_cases_environment(const std::string& name);
bool is_alignment_environment(const std::string& name);
bool is_big_delim_command(const std::string& cmd);
int get_big_delim_size(const std::string& cmd);
bool is_display_style_command(const std::string& cmd);
bool is_fraction_variant(const std::string& cmd);
bool is_over_under_arrow(const std::string& cmd);
bool is_mod_command(const std::string& cmd);
bool is_stack_command(const std::string& cmd);
bool is_phantom_command(const std::string& cmd);
bool is_color_command(const std::string& cmd);
bool is_cancel_command(const std::string& cmd);
bool is_extensible_arrow(const std::string& cmd);

// Get Unicode codepoint for a command (returns 0 if not found)
uint32_t command_to_codepoint(const std::string& cmd);

// Get space amount in em units for a space command
double get_space_amount(const std::string& cmd);

// Get Unicode codepoint for a delimiter (handles both raw chars and command names)
// Returns 0 for empty string or "." (null delimiter)
uint32_t get_delimiter_codepoint(const std::string& delim);

} // namespace math
