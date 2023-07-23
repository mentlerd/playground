
#include <string_view>
#include <array>

struct Opcode {
	std::string_view mnemonic;

	/// Pattern is a human readable description of the incoming bit pattern
	///  that the opcode matches against. 
	///
	/// Unlike most byte based notations in ISA documentation, the pattern is
	///  satisfied left to right, and is matched against a bitstream, as opposed
	///  to a byte stream. As such literal bit patterns are "backwards" compared 
	///  to the 'natural' number notation.
	///
	/// To indicate that a pattern expects a specific bit, you should write
	///  a literal '0' or '1' character. 
	/// 
	/// To consume incoming bits from the stream into placeholder values in 
	///  the mnemonic, write the placeholder's name, followed by an inclusive
	///  bitrange describing which bits to fill.
	///
	/// As a quick sanity check, one may use the ';' character to check that
	///  the pattern is at a byte boundary
	///
	/// Whitespace characters are ignored by the parser, and may be used to
	///  produce a more legible syntax.
	std::string_view pattern;
};

static constexpr std::array g_opcodes = {
	Opcode{"ACALL addr11",       "10001 addr11[8-10]; addr11[0-7];"},
	Opcode{"ADD A,Rn",           "Rn[0-2] 10100;"},
	Opcode{"ADD A,direct",       "10100100; direct[0-7];"},
	Opcode{"ADD A,@Ri",          "Ri[0-0] 1100100;"},
	Opcode{"ADD A,#data",        "00100100; data[0-7];"},
	Opcode{"ADDC A,Rn",          "Rn[0-2] 11100;"},
	Opcode{"ADDC A,direct",      "10101100; direct[0-7];"},
	Opcode{"ADDC A,@Ri",         "Ri[0-0] 1101100;"},
	Opcode{"ADDC A,#data",       "00101100; data[0-7];"},
	Opcode{"AJMP addr11",        "10000 addr11[8-10]; addr11[0-7];"},
	Opcode{"ANL A,Rn",           "Rn[0-2] 11010;"},
	Opcode{"ANL A,direct",       "10101010; direct[0-7];"},
	Opcode{"ANL A,@Ri",          "Ri[0-0] 1101010;"},
	Opcode{"ANL A,#data",        "00101010; data[0-7];"},
	Opcode{"ANL direct,A",       "01001010; direct[0-7];"},
	Opcode{"ANL direct,#data",   "11001010; direct[0-7]; data[0-7];"},
	Opcode{"ANL C,bit",          "01000001; bit[0-7];"},
	Opcode{"ANL C,/bit",         "00001101; bit[0-7];"},
	Opcode{"CJNE A,direct,rel",  "10101101; direct[0-7]; rel[0-7];"},
	Opcode{"CJNE A,#data,rel",   "00101101; data[0-7]; rel[0-7];"},
	Opcode{"CJNE Rn,#data,rel",  "Rn[0-2] 11101; data[0-7]; rel[0-7];"},
	Opcode{"CJNE @Ri,#data,rel", "Ri[0-0] 1101101; data[0-7]; rel[0-7];"},
	Opcode{"CLR A",              "00100111;"},
	Opcode{"CLR C",              "11000011;"},
	Opcode{"CLR bit",            "01000011; bit[0-7];"},
	Opcode{"CPL A",              "00101111;"},
	Opcode{"CPL C",              "11001101;"},
	Opcode{"CLR bit",            "01001101; bit[0-7];"},
	Opcode{"DA A",               "00101011;"},
	Opcode{"DEC A",              "00101000;"},
	Opcode{"DEC Rn",             "Rn[0-2] 11000;"},
	Opcode{"DEC direct",         "10101000; direct[0-7];"},
	Opcode{"DEC @Ri",            "Ri[0-0] 1101000;"},
	Opcode{"DIV AB",             "00100001;"},
	Opcode{"DJNZ Rn,rel",        "Rn[0-2] 11011; rel[0-7];"},
	Opcode{"DJNZ direct,rel",    "10101011; direct[0-7]; rel[0-7];"},
	Opcode{"INC A",              "00100000;"},
	Opcode{"INC Rn",             "Rn[0-2] 10000;"},
	Opcode{"INC direct",         "10100000; direct[0-7];"},
	Opcode{"INC @Ri",            "Ri[0-0] 1100000;"},
	Opcode{"INC DPTR",           "11000101;"},
	Opcode{"JB bit,rel",         "00000100; bit[0-7]; rel[0-7];"},
	Opcode{"JBC bit,rel",        "00001000; bit[0-7]; rel[0-7];"},
	Opcode{"JC rel",             "00000010; rel[0-7];"},
	Opcode{"JMP @A+DPTR",        "11001110;"},
	Opcode{"JNB bit,rel",        "00001100; bit[0-7]; rel[0-7];"},
	Opcode{"JNC rel",            "00001010; rel[0-7];"},
	Opcode{"JNZ rel",            "00001110; rel[0-7];"},
	Opcode{"JZ rel",             "00000110; rel[0-7];"},
	Opcode{"LCALL addr16",       "01001000; addr16[8-15]; addr16[0-7];"},
	Opcode{"LJMP addr16",        "01000000; addr16[8-15]; addr16[0-7];"},
	Opcode{"MOV A,Rn",           "Rn[0-2] 10111;"},
	Opcode{"MOV A,direct",       "10100111; direct[0-7];"},
	Opcode{"MOV A,@Ri",          "Ri[0-0] 1100111;"},
	Opcode{"MOV A,#data",        "00101110; data[0-7];"},
	Opcode{"MOV Rn,A",           "Rn[0-2] 11111;"},
	Opcode{"MOV Rn,direct",      "Rn[0-2] 10101; direct[0-7];"},
	Opcode{"MOV Rn,#data",       "Rn[0-2] 11110; data[0-7];"},
	Opcode{"MOV direct,A",       "1010111; direct[0-7];"},
	Opcode{"MOV direct,Rn",      "Rn[0-2] 10001; direct[0-7];"},
	Opcode{"MOV src,dst",        "10100001; src[0-7]; dst[0-7];"},
	Opcode{"MOV direct,@Ri",     "Ri[0-0]; 1100001; direct[0-7];"},
	Opcode{"MOV direct,#data",   "10101110; direct[0-7]; data[0-7];"},
	Opcode{"MOV @Ri,A",          "Ri[0-0] 1101111;"},
	Opcode{"MOV @Ri,direct",     "Ri[0-0] 1100101; direct[0-7];"},
	Opcode{"MOV @Ri,#data",      "Ri[0-0] 1101110; data[0-7];"},
	Opcode{"MOV C,bit",          "01000101; bit[0-7];"},
	Opcode{"MOV bit,C",          "01001001; bit[0-7];"},
	Opcode{"MOV DPTR,#data16",   "00001001; data16[8-15]; data16[0-7];"},
	Opcode{"MOVC A,@A+DPTR",     "11001001;"},
	Opcode{"MOVC A,@A+PC",       "11000001;"},
	Opcode{"MOVX A,@Ri",         "Ri[0-0] 1000111;"},
	Opcode{"MOVX A,@DPTR",       "00000111;"},
	Opcode{"MOVX @Ri,A",         "Ri[0-0] 1001111;"},
	Opcode{"MOVX @DPTR,A",       "00001111;"},
	Opcode{"MUL AB",             "00100101;"},
	Opcode{"NOP",                "00000000;"},
	Opcode{"ORL A,Rn",           "Rn[0-2] 10010;"},
	Opcode{"ORL A,direct",       "10100010; direct[0-7];"},
	Opcode{"ORL A,@Ri",          "Ri[0-0] 1100010;"},
	Opcode{"ORL A,#data",        "00100010; data[0-7];"},
	Opcode{"ORL direct,A",       "01000001; data[0-7];"},
	Opcode{"ORL direct,#data",   "11000010; direct[0-7]; data[0-7];"},
	Opcode{"ORL C,bit",          "01001110; bit[0-7];"},
	Opcode{"ORL C,/bit",         "00000101; bit[0-7];"},
	Opcode{"POP direct",         "00001011; direct[0-7];"},
	Opcode{"PUSH direct",        "00000011; direct[0-7];"},
	Opcode{"RET",                "01000100;"},
	Opcode{"RETI",               "01001100;"},
	Opcode{"RL A",               "11000100;"},
	Opcode{"RLC A",              "11001100;"},
	Opcode{"RR A",               "11000000;"},
	Opcode{"RRC A",              "11001000;"},
	Opcode{"SETB C",             "11001011;"},
	Opcode{"SETB bit",           "01001011; bit[0-7];"},
	Opcode{"SJMP rel",           "00000001; rel[0-7];"},
	Opcode{"SUBB A,Rn",          "Rn[0-2] 11001;"},
	Opcode{"SUBB A,direct",      "10101001; direct[0-7];"},
	Opcode{"SUBB A,@Ri",         "Ri[0-0] 1101001;"},
	Opcode{"SUBB A,#data",       "00101001; data[0-7];"},
	Opcode{"SWAP A",             "00100011;"},
	Opcode{"XCH A,Rn",           "Rn[0-2] 10011;"},
	Opcode{"XCH A,direct",       "10100011; direct[0-7];"},
	Opcode{"XCH A,@Ri",          "Ri[0-0] 1100011;"},
	Opcode{"XCHD A,@Ri",         "Ri[0-0] 1101011;"},
	Opcode{"XRL A,Rn",           "Rn[0-2] 10110;"},
	Opcode{"XRL A,direct",       "10100110; direct[0-7];"},
	Opcode{"XRL A,@Ri",          "Ri[0-0] 1100110;"},
	Opcode{"XRL A,#data",        "00100110; data[0-7];"},
	Opcode{"XRL direct,A",       "01000110; direct[0-7];"},
	Opcode{"XRL direct,#data",   "11000110; direct[0-7]; data[0-7];"},
};
