// [CHOSEN, not recovered] See D3dShaderAssembler.h. Covers the instruction set the shipped effects
// use; anything else is refused by name rather than assembled wrongly.
#include "D3dShaderAssembler.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>

namespace portshim {
namespace {

using std::string;

enum RegisterType { kTemp = 0, kInput = 1, kConst = 2, kTexture = 3 };

struct Opcode { unsigned code; int sources; bool hasDest; };

const std::map<string, Opcode>& opcodes()
{
    static const std::map<string, Opcode> table = {
        {"nop", {0, 0, false}},   {"mov", {1, 1, true}},    {"add", {2, 2, true}},
        {"sub", {3, 2, true}},    {"mad", {4, 3, true}},    {"mul", {5, 2, true}},
        {"rcp", {6, 1, true}},    {"rsq", {7, 1, true}},    {"dp3", {8, 2, true}},
        {"dp4", {9, 2, true}},    {"min", {10, 2, true}},   {"max", {11, 2, true}},
        {"lrp", {18, 3, true}},   {"frc", {19, 1, true}},   {"texcoord", {64, 0, true}},
        {"texcrd", {64, 1, true}},{"texkill", {65, 0, true}},{"tex", {66, 0, true}},
        {"texld", {66, 1, true}}, {"texm3x2pad", {71, 1, true}}, {"texm3x2tex", {72, 1, true}},
        {"texm3x3pad", {73, 1, true}}, {"texm3x3tex", {74, 1, true}},
        {"cnd", {80, 3, true}},   {"cmp", {88, 3, true}},   {"bem", {89, 2, true}},
    };
    return table;
}

unsigned registerBits(unsigned type, unsigned index)
{
    return 0x80000000u | (index & 0x7FFu) | ((type & 7u) << 28) | ((type & 0x18u) << 8);
}

// Removes block and line comments; `;` also starts a comment in shader assembly.
string stripComments(const string& text)
{
    string out;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text.compare(i, 2, "/*") == 0)
        {
            const size_t e = text.find("*/", i + 2);
            if (e == string::npos) break;
            i = e + 1;
        }
        else if (text.compare(i, 2, "//") == 0 || text[i] == ';')
        {
            while (i < text.size() && text[i] != '\n') ++i;
            out.push_back('\n');
        }
        else
            out.push_back(text[i]);
    }
    return out;
}

// Parses r0 / c12 / t3 / v1, leaving `at` after the digits.
bool parseRegister(const string& s, size_t& at, unsigned* type, unsigned* index)
{
    if (at >= s.size()) return false;
    switch (s[at])
    {
    case 'r': *type = kTemp; break;
    case 'v': *type = kInput; break;
    case 'c': *type = kConst; break;
    case 't': *type = kTexture; break;
    default: return false;
    }
    size_t end = at + 1;
    while (end < s.size() && std::isdigit(static_cast<unsigned char>(s[end]))) ++end;
    if (end == at + 1) return false;
    *index = static_cast<unsigned>(std::atoi(s.c_str() + at + 1));
    at = end;
    return true;
}

int component(char c)
{
    switch (c)
    {
    case 'x': case 'r': return 0;
    case 'y': case 'g': return 1;
    case 'z': case 'b': return 2;
    case 'w': case 'a': return 3;
    default: return -1;
    }
}

bool parseDest(const string& s, unsigned modifierBits, unsigned* token)
{
    size_t at = 0;
    unsigned type = 0, index = 0;
    if (!parseRegister(s, at, &type, &index)) return false;
    unsigned mask = 0xF;
    if (at < s.size())
    {
        if (s[at] != '.') return false;
        mask = 0;
        for (size_t i = at + 1; i < s.size(); ++i)
        {
            const int c = component(s[i]);
            if (c < 0) return false;
            mask |= 1u << c;
        }
    }
    *token = registerBits(type, index) | (mask << 16) | modifierBits;
    return true;
}

bool parseSource(const string& text, unsigned* token)
{
    string s = text;
    bool negate = false, complement = false;
    if (s.compare(0, 2, "1-") == 0) { complement = true; s.erase(0, 2); }
    if (!s.empty() && s[0] == '-') { negate = true; s.erase(0, 1); }
    size_t at = 0;
    unsigned type = 0, index = 0;
    if (!parseRegister(s, at, &type, &index)) return false;
    unsigned modifier = negate ? 1 : 0;
    if (complement) modifier = 6;
    if (at < s.size() && s[at] == '_')
    {
        size_t end = s.find('.', at);
        if (end == string::npos) end = s.size();
        const string name = s.substr(at + 1, end - at - 1);
        at = end;
        if (name == "bias") modifier = negate ? 3 : 2;
        else if (name == "bx2") modifier = negate ? 5 : 4;
        else if (name == "x2") modifier = negate ? 8 : 7;
        else if (name == "dz" || name == "db") modifier = 9;
        else if (name == "dw" || name == "da") modifier = 10;
        else return false;
    }
    unsigned swizzle = 0xE4;
    if (at < s.size())
    {
        if (s[at] != '.') return false;
        const string letters = s.substr(at + 1);
        if (letters.empty() || letters.size() > 4) return false;
        swizzle = 0;
        int last = 0;
        for (size_t i = 0; i < 4; ++i)
        {
            if (i < letters.size())
            {
                last = component(letters[i]);
                if (last < 0) return false;
            }
            swizzle |= static_cast<unsigned>(last) << (2 * i);
        }
    }
    *token = registerBits(type, index) | (swizzle << 16) | (modifier << 24);
    return true;
}

// `mad_x2_sat` gives the mnemonic and the destination's shift and saturate bits.
bool splitMnemonic(const string& word, string* mnemonic, unsigned* modifierBits)
{
    size_t end = word.find('_');
    *mnemonic = word.substr(0, end);
    *modifierBits = 0;
    while (end != string::npos)
    {
        const size_t next = word.find('_', end + 1);
        const string part = word.substr(end + 1, next == string::npos ? string::npos : next - end - 1);
        end = next;
        if (part == "sat") *modifierBits |= 1u << 20;
        else if (part == "pp") *modifierBits |= 2u << 20;
        else if (part == "x2") *modifierBits |= 1u << 24;
        else if (part == "x4") *modifierBits |= 2u << 24;
        else if (part == "x8") *modifierBits |= 3u << 24;
        else if (part == "d2") *modifierBits |= 15u << 24;
        else if (part == "d4") *modifierBits |= 14u << 24;
        else if (part == "d8") *modifierBits |= 13u << 24;
        else return false;
    }
    return true;
}

// `ps.1.4` and `ps_1_4` give the version token.
bool parseVersion(const string& word, unsigned* token)
{
    if (word.size() != 6 || (word[0] != 'p' && word[0] != 'v') || word[1] != 's') return false;
    if ((word[2] != '.' && word[2] != '_') || word[3] < '1' || word[3] > '3') return false;
    if ((word[4] != '.' && word[4] != '_') || !std::isdigit(static_cast<unsigned char>(word[5]))) return false;
    *token = (word[0] == 'p' ? 0xFFFF0000u : 0xFFFE0000u)
        | (static_cast<unsigned>(word[3] - '0') << 8) | static_cast<unsigned>(word[5] - '0');
    return true;
}

std::vector<string> splitOperands(const string& text)
{
    std::vector<string> parts;
    string current;
    for (char c : text)
    {
        if (c == ',') { parts.push_back(current); current.clear(); }
        else if (!std::isspace(static_cast<unsigned char>(c))) current.push_back(c);
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

} // namespace

bool assembleD3dShader(const string& source, std::vector<unsigned>* words, string* error)
{
    const string text = stripComments(source);
    words->clear();
    size_t position = 0;
    while (position < text.size())
    {
        size_t end = text.find('\n', position);
        if (end == string::npos) end = text.size();
        string line = text.substr(position, end - position);
        position = end + 1;

        size_t at = line.find_first_not_of(" \t\r");
        if (at == string::npos) continue;
        bool coissue = false;
        if (line[at] == '+')
        {
            coissue = true;
            at = line.find_first_not_of(" \t", at + 1);
            if (at == string::npos) { *error = "lone + in: " + line; return false; }
        }
        size_t wordEnd = at;
        while (wordEnd < line.size() && !std::isspace(static_cast<unsigned char>(line[wordEnd]))) ++wordEnd;
        string word = line.substr(at, wordEnd - at);
        for (char& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        string rest = line.substr(wordEnd);
        for (char& c : rest) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        unsigned version = 0;
        if (words->empty())
        {
            if (!parseVersion(word, &version)) { *error = "no version line before: " + line; return false; }
            words->push_back(version);
            continue;
        }
        if (word == "phase") { words->push_back(0xFFFDu); continue; }
        std::vector<string> operands = splitOperands(rest);
        if (word == "def")
        {
            unsigned dest = 0;
            if (operands.size() != 5 || !parseDest(operands[0], 0, &dest))
            { *error = "bad def: " + line; return false; }
            words->push_back(81u);
            words->push_back(dest);
            for (int i = 1; i < 5; ++i)
            {
                const float value = static_cast<float>(std::atof(operands[i].c_str()));
                unsigned bits;
                std::memcpy(&bits, &value, 4);
                words->push_back(bits);
            }
            continue;
        }
        string mnemonic;
        unsigned modifiers = 0;
        if (!splitMnemonic(word, &mnemonic, &modifiers)) { *error = "bad modifier: " + line; return false; }
        const auto found = opcodes().find(mnemonic);
        if (found == opcodes().end()) { *error = "instruction not supported: " + line; return false; }
        const Opcode& op = found->second;
        const size_t expected = static_cast<size_t>(op.sources) + (op.hasDest ? 1 : 0);
        if (operands.size() != expected) { *error = "wrong operand count: " + line; return false; }
        words->push_back(op.code | (coissue ? 0x40000000u : 0u));
        for (size_t i = 0; i < operands.size(); ++i)
        {
            unsigned token = 0;
            const bool ok = (i == 0 && op.hasDest) ? parseDest(operands[i], modifiers, &token)
                                                   : parseSource(operands[i], &token);
            if (!ok) { *error = "bad operand " + operands[i] + " in: " + line; return false; }
            words->push_back(token);
        }
    }
    if (words->empty()) { *error = "empty shader block"; return false; }
    words->push_back(0x0000FFFFu);
    return true;
}

} // namespace portshim
