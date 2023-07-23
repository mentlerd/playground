
#include <string_view>
#include <array>
#include <cstdio>
#include <cmath>

struct Opcode {
    static constexpr std::size_t parseInt(std::string_view pattern, std::size_t& i) {
        auto result = size_t(pattern[i]) - '0';
        if (result > 9) {
            throw;
        }

        while (true) {
            if (++i == pattern.size()) {
                break;
            }

            auto raw = size_t(pattern[i]) - '0';
            if (raw > 9) {
                break;
            }

            result = result * 10 + raw;
        }

        return result;
    }

    constexpr Opcode(std::string_view mnemonic, const std::string_view pattern)
    : mnemonic(mnemonic)
    , pattern(pattern)
    {
        std::size_t streamOffset = 0;
        bool didSanityCheck = true;

        for (std::size_t i = 0; i < pattern.size(); i++) {
            const auto chr = pattern[i];

            // Ignore whitespace
            if (chr == ' ') {
                continue;
            }

            // Perform sanity check
            if (streamOffset % 8 == 0) {
                if (chr == ';') {
                    didSanityCheck = true;
                    continue;
                }
                if (!didSanityCheck) {
                    throw;
                }
            } else {
                if (chr == ';') {
                    throw;
                }
                didSanityCheck = false;
            }

            // Parse literals
            if (chr == '0' || chr == '1') {
                streamOffset++;
                continue;
            }

            // Parse bit capture name
            while (pattern[i] != '[') {
                if (++i == pattern.size()) {
                    throw;
                }
            }

            if (++i == pattern.size()) {
                throw;
            }

            // Parse range start
            std::size_t start = parseInt(pattern, i);
            std::size_t end;

            if (pattern[i] == ']') {
                end = start; // Single bit capture
            } else if (pattern[i] == '-') {
                if (++i == pattern.size()) {
                    throw;
                }

                end = parseInt(pattern, i);
            } else {
                throw;
            }

            if (pattern[i] != ']') {
                throw;
            }

            if (start <= end) {
                streamOffset += end - start + 1;
            } else {
                streamOffset += start - end + 1;
            }
        }
    }

	std::string_view mnemonic;
	std::string_view pattern;
};

static constexpr std::array g_opcodes = {
	// Add with Carry
	Opcode{"ADC R,r",           "r[0-2] 1 z[0-1] 11; R[0-2] 01001;"},
	Opcode{"ADC r,#",           "r[0-2] 1 z[0-1] 11; 10010011; #[0-31];"},
	Opcode{"ADC R,(mem)",       "mem[0-3] z[0-1] mem[4] 1; R[0-2] 01001;"},
	Opcode{"ADC (mem),R",       "mem[0-3] z[0-1] mem[4] 1; R[0-2] 11001;"},
	Opcode{"ADC<W> (mem),#",    "mem[0-3] z[0] 0 mem[4] 1; 10011100; #[0-15];"},

	// Add
	Opcode{"ADD R,r",           "r[0-2] 1 z[0-1] 11; R[0-2] 00001;"},
	Opcode{"ADD r,#",           "r[0-2] 1 z[0-1] 11; 00010011; #[0-31];"},
	Opcode{"ADD R,(mem)",       "mem[0-3] z[0-1] mem[4] 1; R[0-2] 00001;"},
	Opcode{"ADD (mem),R",       "mem[0-3] z[0-1] mem[4] 1; R[0-2] 10001;"},
	Opcode{"ADD<W> (mem),#",    "mem[0-3] z[0] 0 mem[4] 1; 00011100; #[0-15];"},

	// And
	Opcode{"AND R,r",           "r[0-2] 1 z[0-1] 11; R[0-2] 00011;"},
	Opcode{"AND r,#",           "r[0-2] 1 z[0-1] 11; 00110011; #[0-31];"},
	Opcode{"AND R,(mem)",       "mem[0-3] z[0-1] mem[4] 1; R[0-2] 00011;"},
	Opcode{"AND (mem),R",       "mem[0-3] z[0-1] mem[4] 1; R[0-2] 10011;"},
	Opcode{"AND<W> (mem),#",    "mem[0-3] z[0] 0 mem[4] 1; 00111100; #[0-15];"},

	// And Carry Flag
	Opcode{"ANDCF #4,r",        "r[0-2] 1 z[0] 011; 00000100; #4[0-3] 0000;"},
	Opcode{"ANDCF A,r",         "r[0-2] 1 z[0] 011; 00010100;"},
	Opcode{"ANDCF #3,(mem)",    "mem[0-3] 11 mem[4] 1; #3[0-2] 00001;"},
	Opcode{"ANDCF A,(mem)",     "mem[0-3] 01 mem[4] 1; 00010100;"},

	// Bit test
	Opcode{"BIT #4,r",          "r[0-2] 1 z[0] 011; 11001100; #4[0-3] 0000;"},
	Opcode{"BIT #3,(mem)",      "m[0-3] 11 mem[4] 1; #3[0-2] 10011;"},

	// Bit Search 1 Backward
	Opcode{"BS1B A,r",          "r[0-2] 11011; 11110000;"},

	// Bit Search 1 Forward
	Opcode{"BS1F A,r",          "r[0-2] 11011; 01110000;"},

	// Call subroutine
	Opcode{"CALL #16",          "00111000; #16[0-15];"},
	Opcode{"CALL #24",          "10111000; #24[0-23];"},
	Opcode{"CALL [cc,]mem",     "mem[0-3] 11 mem[4] 1; cc[0-3] 0111;"},

	// Call Relative
	Opcode{"CALR $+3+d16",      "01111000; d16[0-15];"},

	// Complement Carry Flag
	Opcode{"CCF",               "01001000;"},

	// Change
	Opcode{"CHG #4,r",          "r[0-2] 1 z[0] 011; 01001100; #4[0-3] 0000;"},
	Opcode{"CHG #3,(mem)",      "mem[0-3] 11 mem[4] 1; #3[0-2] 00011;"},

	// Compare
	Opcode{"CP R,r",            "r[0-2] 1 z[0-1] 11; R[0-2] 01111;"},
	Opcode{"CP r,#3",           "r[0-2] 1 z[0] 011; #3[0-2] 11011;"},
	Opcode{"CP r,#",            "r[0-2] 1 z[0-1] 11; 11110011; #[0-31];"},
	Opcode{"CP R,(mem)",        "mem[0-3] z[0-2] mem[4] 1; R[0-2] 01111;"},
	Opcode{"CP (mem),R",        "mem[0-3] z[0-2] mem[4] 1; R[0-2] 11111;"},
	Opcode{"CP<W> (mem),#",     "mem[0-3] z[0] 0 mem[4] 1; 11111100; #[0-15]"},

	// Compare Decrement
	Opcode{"CPD [A/WA,(R-)]",   "R[0-2] 0 z[0] 001; 01101000;"},

	// Compare Decrement Repeat
	Opcode{"CPDR [A/WA,(R-)]",  "R[0-2] 0 z[0] 001; 11101000;"},

	// Compare Increment
	Opcode{"CPI [A/WA,(R+)]",   "R[0-2] 0 z[0] 001; 00101000;"},

	// Compare Increment Repeat
	Opcode{"CPIR [A/WA,(R+)]",  "R[0-2] 0 z[0] 001; 10101000;"},

	// Complement
	Opcode{"CPL r",             "r[0-2] 1 z[0] 011; 01100000;"},

	// Decimal Adjust Accumulator
	Opcode{"DAA r",             "r[0-2] 10011; 00001000;"},

	// Decrement
	Opcode{"DEC #3,r",          "r[0-2] 1 z[0-1] 11; #3[0-2] 10110;"},
	Opcode{"DEC<W> #3,(mem)",   "mem[0-3] z[0] 0 mem[4] 1; #3[0-2] 10110;"},

	// Decrement Register File Pointer
	Opcode{"DECF",              "10110000;"},

	// Disable Interrupt
	Opcode{"DI",                "01100000; 11100000;"},

	// Divide
	Opcode{"DIV RR,r",          "r[0-2] 1 z[0] 011; RR[0-2] 01010;"},
	Opcode{"DIV rr,#",          "r[0-2] 1 z[0] 011; 01010000; #[0-15];"},
	Opcode{"DIV RR,(mem)",      "mem[0-3] z[0] 0 mem[4] 1; RR[0-2] 01010;"},

	// Divide Signed
	Opcode{"DIVS RR,r",         "r[0-2] 1 z[0] 011; RR[0-2] 11010;"},
	Opcode{"DIVS rr,#",         "r[0-2] 1 z[0] 011; 11010000;"},
	Opcode{"DIVS RR,(mem)",     "mem[0-3] z[0] 0 mem[4] 1; RR[0-2] 11010;"},

	// Decrement and Jump if Non Zero
	Opcode{"DJNZ [r,]$+3+d8",   "r[0-2] 1 z[0] 011; 00111000; d8[0-7];"},

	// Enable Interrupt
	Opcode{"EI [#3]",           "01100000; #3[0-2] 00000;"},

	// Exchange
	Opcode{"EX F,F'",           "01101000;"},
	Opcode{"EX R,r",            "r[0-2] 1 z[0-1] 11; R[0-2] 01100;"},
	Opcode{"EX (mem),r",        "mem[0-3] z[0-1] mem[4] 1; R[0-2] 01100;"},

	// Extend Sign
	Opcode{"EXTS r",            "r[0-2] 1 z[0-1] 11; 11001000;"},

	// Extend Zero
	Opcode{"EXTZ r",            "r[0-2] 1 z[0-1] 11; 01001000;"},

	// Halt CPU
	Opcode{"HALT",              "10100000;"},

	// Increment
	Opcode{"INC #3, r",         "r[0-2] 1 z[0-1] 11; #3[0-2] 00110;"},
	Opcode{"INC<W> #3,(mem)",   "mem[0-3] z[0] 0 mem[4] 1; #3[0-2] 00110;"},

	// Increment Register File Pointer
	Opcode{"INCF",              "00110000;"},

	// Jump
	Opcode{"JP #16",            "01011000; #16[0-15];"},
	Opcode{"JP #24",            "11011000; #24[0-23];"},
	Opcode{"JP [cc,]mem",       "mem[0-3] 11 mem[4] 1; cc[0-2] 1011;"},

	// Jump Relative
	Opcode{"JR [cc,]$+2+d8",    "cc[0-2] 0110; d8[0-7];"},
	Opcode{"JRL [cc,]$+2+d16",  "cc[0-2] 1110; d16[0-15];"},

	// Load
	Opcode{"LD R,r",            "r[0-2] 1 z[0-1] 11; R[0-2] 10001;"},
	Opcode{"LD r,R",            "r[0-2] 1 z[0-1] 11; R[0-2] 11001;"},
	Opcode{"LD r,#3",           "r[0-2] 1 z[0-1] 11; #3[0-2] 10101;"},
	Opcode{"LD R,#",            "R[0-2] 0 z[0-2] 0; #[0-31];"},
	Opcode{"LD r,#",            "r[0-2] 1 z[0-2] 11; 11000000; #[0-31];"},
	Opcode{"LD R,(mem)",        "mem[0-3] z[0-2] mem[4] 1; R[0-2] 00100;"},
	Opcode{"LD (mem),R",        "mem[0-3] 11 mem[4] 1; R[0-2] 0 z[0-2] 10;"},
	Opcode{"LD<W> (#8),#",      "0 z[0] 010000; #8[0-7]; #[0-15];"},
	Opcode{"LD<W> (mem),#",     "mem[0-3] 11 mem[4] 1; 0 z[0] 000000; #[0-15];"},
	Opcode{"LD<W> (#16),(mem)", "mem[0-3] z[0] 0 mem[4] 1; 1001100; #16[0-15];"},
	Opcode{"LD<W> (mem),(#16)", "mem[0-3] 11 mem[4] 1; 0 z[0] 10100; #16[0-15];"},

	// Load Address
	Opcode{"LDA R,mem",         "mem[0-3] 11 mem[4] 1; R[0-2] 0 s[0] 100;"},

	// Load Address Relative
	Opcode{"LDAR R,$+4+d16",    "11001111; 11001000; d16[0-15]; R[0-2] 0 s[0] 100;"},

	// Load Control Register
	Opcode{"LDC cr,r",          "r[0-2] 1 z[0-1] 11; 01110100; cr[0-7];"},
	Opcode{"LDC r,cr",          "r[0-2] 1 z[0-1] 11; 11110100; cr[0-7];"},

	// Load Carry Flag
	Opcode{"LDCF #4,r",         "r[0-2] 1 z[0] 011; 11000100; #4[0-3] 0000;"},
	Opcode{"LDCF A,r",          "r[0-2] 1 z[0] 011; 11010100;"},
	Opcode{"LDCF #3,(mem)",     "mem[0-3] 11 mem[4] 1; #3[0-2] 11001;"},
	Opcode{"LDCF A,(mem)",      "mem[0-3] 11 mem[4] 1; 11010100;"},

	// Load Decrement
	Opcode{"LDD<W> [(XDE-),(XHL-)]",  "1100 z[0] 001; 01001000;"},
	Opcode{"LDD<W> (XIX-),(XIY-)",    "1010 z[0] 001; 01001000;"},

	// Load Decrement Repeat
	Opcode{"LDDR<W> [(XDE-),(XHL-)]", "1100 z[0] 001; 11001000;"},
	Opcode{"LDDR<W> (XIX-),(XIY-)",   "1010 z[0] 001; 11001000;"},

	// Load Register File Pointer
	Opcode{"LDF #3",            "11101000; #3[0-2] 00000;"},

	// Load Increment
	Opcode{"LDI<W> [(XDE+),(XHL+)]",  "1100 z[0] 001; 00001000;"},
	Opcode{"LDI<W> (XIX+),(XIY+)",    "1010 z[0] 001; 00001000;"},

	// Load Increment Repeat
	Opcode{"LDIR<W> [(XDE+),(XHL+)]", "1100 z[0] 001; 10001000;"},
	Opcode{"LDIR<W> (XIX+),(XIY+)",   "1010 z[0] 001; 10001000;"},

	// Load eXtract
	Opcode{"LDX (#8),#",         "11101111; 00000000; #8[0-7]; 00000000; #[0-7]; 00000000;"},

	// Link
	Opcode{"LINK r,d16",         "r[0-2] 10111; 00110000; d16[0-15];"},

	// Maximum
	Opcode{"MAX",                "00100000;"},

	// Modulo Decrement
	Opcode{"MDEC1 #,r",          "r[0-2] 11011; 00111100; #[0-15];"},
	Opcode{"MDEC2 #,r",          "r[0-2] 11011; 10111100; #[0-15];"},
	Opcode{"MDEC4 #,r",          "r[0-2] 11011; 01111100; #[0-15];"},

	// Minimum
	Opcode{"MIN",                "00100000;"},

	// Modulo Increment
	Opcode{"MINC1 #,r",          "r[0-2] 11011; 00011100; #[0-15];"},
	Opcode{"MINC2 #,r",          "r[0-2] 11011; 10011100; #[0-15];"},
	Opcode{"MINC4 #,r",          "r[0-2] 11011; 01011100; #[0-15];"},

	// Mirror
	Opcode{"MIRR r",             "r[0-2] 11011; 01101000;"},

	// Multiply
	Opcode{"MUL RR,r",           "r[0-2] 1 z[0] 011; RR[0-2] 00010;"},
	Opcode{"MUL rr,#",           "rr[0-2] 1 z[0] 011; 00010000; #[0-15];"},
	Opcode{"MUL RR,(mem)",       "mem[0-3] z[0] 0 mem[4] 1; RR[0-2] 00010;"},

	// Multiply and Add
	Opcode{"MULA rr",            "rr[0-2] 11011; 1001100;"},

	// Multiply Signed
	Opcode{"MULS RR,r",          "r[0-2] 1 z[0] 011; R[0-2] 10010;"},
	Opcode{"MULS rr,#",          "rr[0-2] 1 z[0] 011; 10010000;"},
	Opcode{"MULS RR,(mem)",      "mem[0-3] z[0] 0 mem[4] 1; RR[0-2] 10010;"},

	// Negate
	Opcode{"NEG r",              "r[0-2] 1 z[0] 011; 11100000;"},

	// No Operation
	Opcode{"NOP",                "00000000;"},

	// Normal
	Opcode{"NORMAL",             "10000000;"},

	// Logical OR
	Opcode{"OR R,r",             "r[0-2] 1 z[0-1] 11; R[0-2] 00111;"},
	Opcode{"OR r,#",             "r[0-2] 1 z[0-1] 11; 01110011; #[0-31];"},
	Opcode{"OR R,(mem)",         "mem[0-3] z[0-1] mem[4] 1; R[0-2] 00111;"},
	Opcode{"OR (mem),R",         "mem[0-3] z[0-1] mem[4] 1; R[0-2] 10111;"},
	Opcode{"OR<W> (mem),#",      "mem[0-3] z[0] 0 mem[4] 1; 01111100; #[0-15];"},

	// OR Carry Flag
	Opcode{"ORCF #4,r",          "r[0-2] 1 z[0] 0 11; 10000100; #4[0-3] 0000;"},
	Opcode{"ORCF A,r",           "r[0-2] 1 z[0] 011; 10010100;"},
	Opcode{"ORCF #3,(mem)",      "mem[0-3] 11 mem[4] 1; #3[0-3] 10001;"},
	Opcode{"ORCF A,(mem)",       "mem[0-3] 11 mem[4] 1; 10010100;"},

	// Pointer Adjust Accumulator
	Opcode{"PAA r",              "r[0-2] 1 z[0-1] 11; 00101000;"},

	// Pop
	Opcode{"POP F",              "10011000;"},
	Opcode{"POP A",              "10101000;"},
	Opcode{"POP R",              "R[0-2] 1 s[0] 010;"},
	Opcode{"POP r",              "r[0-2] 1 z[0-1] 11;"},
	Opcode{"POP<W> (mem)",       "mem[0-3] 11 mem[4] 1; 0 z[0] 100000;"},

	// Pop SR
	Opcode{"POP SR",             "11000000;"},

	// Push SH
	Opcode{"PUSH SR",            "01000000;"},

	// Push
	Opcode{"PUSH F",             "00011000;"},
	Opcode{"PUSH A",             "00101000;"},
	Opcode{"PUSH R",             "R[0-2] 1 s[0] 100;"},
	Opcode{"PUSH r",             "r[0-2] 1 z[0-1] 11; 00100000;"},
	Opcode{"PUSH<W> #",          "1 z[0] 010000; #[0-15];"},
	Opcode{"PUSH<W> (mem)",      "mem[0-3] z[0] 0 mem[4] 1; 00100000;"},

	// Reset Carry Flag
	Opcode{"RCF",                "00010000"},

	// Reset
	Opcode{"RES #4,r",           "r[0-2] 1 z[0] 011;"},
	Opcode{"RES #3,(mem)",       "mem[0-3] 11 mem[4] 1; #3[0-2] 01101;"},

	// Return
	Opcode{"RET",                "01110000;"},
	Opcode{"RET cc",             "00001101; cc[0-3] 1111;"},

	// Return and Deallocate
	Opcode{"RETD d16",           "11110000; d16[0-15];"},

	// Return from Interrupt
	Opcode{"RETI",               "11100000"},

	// Rotate Left
	Opcode{"RL #4,r",            "r[0-2] 1 z[0-1] 11; 01010111; #4[0-3] 0000;"},
	Opcode{"RL A,r",             "r[0-2] 1 z[0-1] 11; 01011111;"},
	Opcode{"RL<W> (mem)",        "mem[0-3] z[0] 0 mem[4] 1; 01011110;"},

	// Rotate Left without Carry
	Opcode{"RLC #4,r",           "r[0-2] 1 z[0-1] 11; 00010111; #4[0-3] 0000;"},
	Opcode{"RLC A,r",            "r[0-2] 1 z[0-1] 11; 00011111;"},
	Opcode{"RLC<W> (mem)",       "mem[0-3] z[0] 0 mem[4] 1; 00011110;"},

	// Rotate Left Digit
	Opcode{"RLD [A,](mem)",      "mem[0-3] 00 mem[4] 1; 01100000;"},

	// Rotate Right
	Opcode{"RR #4,r",            "r[0-2] 1 z[0-1] 11; 11010111; #4[0-3] 0000;"},
	Opcode{"RR A,r",             "r[0-2] 1 z[0-1] 11; 11011111;"},
	Opcode{"RR<W> (mem)",        "mem[0-3] z[0] 0 mem[4] 1; 11011110;"},

	// Rotate Right without Carry
	Opcode{"RRC #4,r",           "r[0-2] 1 z[0-1] 11; 10010111; #4[0-3] 0000;"},
	Opcode{"RRC A,r",            "r[0-2] 1 z[0-1] 11; 10011111;"},
	Opcode{"RRC<W> (mem)",       "mem[0-3] z[0] 0 mem[4] 1; 10011110;"},

	// Rotate Right Digit
	Opcode{"RRD [A,](mem)",      "mem[0-3] 00 mem[4] 1; 11100000;"},

	// Subtract with Carry
	Opcode{"SBC R,r",            "r[0-2] 1 z[0-1] 11; R[0-2] 01101;"},
	Opcode{"SBC r,#",            "r[0-2] 1 z[0-1] 11; 11010011; #[0-31];"},
	Opcode{"SBC R,(mem)",        "mem[0-3] z[0-1] mem[4] 1; R[0-2] 01101;"},
	Opcode{"SBC (mem),R",        "mem[0-3] z[0-1] mem[4] 1; R[0-2] 11101;"},
	Opcode{"SBC<W> (mem),#",     "mem[0-3] z[0] 0 mem[4] 1; 11011100; #[0-15];"},

	// Set Condition Code
	Opcode{"SCC cc,r",           "r[0-2] 1 z[0] 011; cc[0-2] 1110;"},

	// Set Carry Flag
	Opcode{"SCF",                "10001000;"},

	// Set
	Opcode{"SET #4,r",           "r[0-2] 1 z[0] 011; 10001100; #4[0-3] 0000;"},
	Opcode{"SET #3,(mem)",       "mem[0-3] 11 mem[4] 1; #3[0-2] 11101;"},

	// Shift Left Arithmetic
	Opcode{"SLA #4,r",           "r[0-2] 1 z[0-1] 11; 00110111; #4[0-3] 0000;"},
	Opcode{"SLA A,r",            "r[0-2] 1 z[0-1] 11; 00111111;"},
	Opcode{"SLA<W> (mem)",       "mem[0-3] z[0] 0 mem[4] 1; 00111110;"},

	// Shift Left Logical
	Opcode{"SLL #4,r",           "r[0-2] 1 z[0-1] 11; 01110111; #4[0-2] 0000;"},
	Opcode{"SLL A,r",            "r[0-2] 1 z[0-1] 11; 01111111;"},
	Opcode{"SLL<W> (mem)",       "mem[0-3] z[0] 0 mem[4] 1; 01111110;"},

	// Shift Right Arithmetic
	Opcode{"SRA #4,r",           "r[0-2] 1 z[0-1] 11; 10110111; #4[0-3] 0000;"},
	Opcode{"SRA A,r",            "r[0-2] 1 z[0-1] 11; 10111111;"},
	Opcode{"SRA<W> (mem)",       "mem[0-3] z[0] 0 mem[4] 1; 10111110;"},

	// Shift Right Logical
	Opcode{"SRL #4,r",           "r[0-2] 1 z[0-1] 11; 11110111; #4[0-3] 0000;"},
	Opcode{"SRL A,r",            "r[0-2] 1 z[0-1] 11; 11111111;"},
	Opcode{"SRL<W> (mem)",       "mem[0-3] z[0] 0 mem[4] 1; 11111110;"},

	// Store Carry Flag
	Opcode{"STCF #4,r",          "r[0-2] 1 z[0] 011; 00100100; #4[0-3] 0000;"},
	Opcode{"STCF A,r",           "r[0-2] 1 z[0] 011; 00110100;"},
	Opcode{"STCF #3,(mem)",      "mem[0-3] 11 mem[4] 1; #3[0-2] 00101;"},
	Opcode{"STCF A,(mem)",       "mem[0-3] 11 mem[4] 1; 00110100;"},

	// Subtract
	Opcode{"SUB R,r",            "r[0-2] 1 z[0-1] 11; R[0-2] 00101;"},
	Opcode{"SUB r,#",            "r[0-2] 1 z[0-1] 11; 01010011; #[0-31];"},
	Opcode{"SUB R,(mem)",        "mem[0-3] z[0-1] mem[4] 1; R[0-2] 10101;"},
	Opcode{"SUB (mem),R",        "mem[0-3] z[0-1] mem[4] 1; R[0-2] 10101;"},
	Opcode{"SUB<W> (mem),#",     "mem[0-3] z[0] 0 mem[4] 1; 01011100; #[0-15];"},

	// Software Interrupt
	Opcode{"SWI [#3]",           "#3[0-2] 11111;"},

	// Test and Set
	Opcode{"TSET #4,r",          "r[0-2] 1 z[0-1] 11; 00100011;"},
	Opcode{"TSET #3,(mem)",      "mem[0-3] 11 mem[4] 1; 00110101;"},

	// Unlink
	Opcode{"UNLK r",             "r[0-2] 10111; 10110000;"},

	// Excusive OR
	Opcode{"XOR R,r",            "r[0-2] 1 z[0-1] 11; R[0-2] 01011;"},
	Opcode{"XOR r,#",            "r[0-2] 1 z[0-1] 11; 10110011; #[0-31];"},
	Opcode{"XOR R,(mem)",        "mem[0-3] z[0-1] mem[4] 1; R[0-2] 01011;"},
	Opcode{"XOR (mem),R",        "mem[0-3] z[0-1] mem[4] 1; R[0-2] 11011;"},
	Opcode{"XOR<W> (mem),#",     "mem[0-3] z[0] 0 mem[4] 1; 10111100; #[0-15];"},

	// Exclusive OR Carry Flag
	Opcode{"XORCF #4,r",         "r[0-2] 1 z[0] 011; 01000100; #4[0-3] 0000;"},
	Opcode{"XORCF A,r",          "r[0-2] 1 z[0] 011; 01010100;"},
	Opcode{"XORCF #3,(mem)",     "mem[0-3] 11 mem[4] 1; #3[0-2] 01001;"},
	Opcode{"XORCF A,(mem)",      "mem[0-3] 11 mem[4] 1; 01010100;"},

	// Zero flag to Carry Flag
	Opcode{"ZCF",                "11001000;"},
};

int main() {
    return 0;
}
