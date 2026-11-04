// [CHOSEN, not recovered] A D3DX effect runtime for builds without d3dx9_25.dll. It reads an .fx
// source, keeps its parameters, techniques and pass states, compiles each entry point on first use
// with vkd3d-shader to D3D bytecode, and binds constants by reading the bytecode's constant table.
// The renderer only sees ID3DXEffect's vtable, so every slot keeps its D3DX position.
// Assembly shader blocks are not supported: a pass using one fails validation, visibly.
// [RETIRE-WHEN: portable-d3dx] Remove with a native renderer path that no longer uses effects.

#include "D3dShaderAssembler.h"
#include <d3d9.h>

#include <vkd3d_shader.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using std::string;

enum { PC_SCALAR, PC_VECTOR, PC_MATRIX_ROWS, PC_MATRIX_COLUMNS, PC_OBJECT, PC_STRUCT };
enum { PT_VOID, PT_BOOL, PT_INT, PT_FLOAT, PT_STRING, PT_TEXTURE, PT_TEXTURE1D, PT_TEXTURE2D,
       PT_TEXTURE3D, PT_TEXTURECUBE, PT_SAMPLER, PT_SAMPLER1D, PT_SAMPLER2D, PT_SAMPLER3D,
       PT_SAMPLERCUBE, PT_PIXELSHADER = 15, PT_VERTEXSHADER = 16 };

const long kOk = 0;
const long kFail = static_cast<long>(0x80004005u);

string lower(string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

struct StateAssign
{
    string name;    // lower case, without its index
    int    index;
    string value;
};

// A parameter, or an annotation, which D3DX hands out through the same handles.
struct Param
{
    string name, semantic, text;
    unsigned cls = PC_SCALAR, type = PT_FLOAT, rows = 1, columns = 1, elements = 0;
    std::vector<unsigned char> data;
    std::vector<std::unique_ptr<Param>> annotations;
    std::vector<std::unique_ptr<Param>> children;   // the elements of a string array
    std::vector<StateAssign> samplerStates;
    IDirect3DBaseTexture9* texture = nullptr;
    bool isObject() const { return cls == PC_OBJECT; }
    unsigned count() const { return (elements ? elements : 1) * rows * columns; }
};

struct ShaderConstant
{
    string name;
    unsigned set, reg, count, cls, rows, columns;
};

struct Shader
{
    bool   pixel = false;
    string profile, entry, args, assembly;
    int    state = 0;   // 0 not built, 1 built, -1 failed
    void*  object = nullptr;
    std::vector<ShaderConstant> constants;
};

struct ShaderRef
{
    bool   present = false;     // the pass assigns this stage at all
    Shader* shader = nullptr;   // null with present means NULL
    string expression;          // an indirection through a shader variable or array
};

struct Pass
{
    string name;
    std::vector<std::unique_ptr<Param>> annotations;
    std::vector<StateAssign> states;
    ShaderRef vertex, pixel;
};

struct Technique
{
    string name;
    std::vector<std::unique_ptr<Param>> annotations;
    std::vector<std::unique_ptr<Pass>> passes;
};

struct Function
{
    string ret, params, semantic;
};

// ----- expressions ------------------------------------------------------------------------------

struct Evaluator
{
    const string& s;
    size_t i = 0;
    bool   bad = false;
    explicit Evaluator(const string& text) : s(text) {}
    void ws() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; }
    double primary()
    {
        ws();
        if (i >= s.size()) { bad = true; return 0; }
        if (s[i] == '(')
        {
            ++i;
            const double v = expression();
            ws();
            if (i < s.size() && s[i] == ')') ++i; else bad = true;
            return v;
        }
        if (s[i] == '-') { ++i; return -primary(); }
        if (s[i] == '+') { ++i; return primary(); }
        if (s[i] == '~') { ++i; return static_cast<double>(~static_cast<long long>(primary())); }
        if (s[i] == '!') { ++i; return primary() == 0 ? 1 : 0; }
        if (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.')
        {
            char* end = nullptr;
            double v;
            if (s[i] == '0' && i + 1 < s.size() && (s[i + 1] == 'x' || s[i + 1] == 'X'))
                v = static_cast<double>(std::strtoull(s.c_str() + i, &end, 16));
            else
                v = std::strtod(s.c_str() + i, &end);
            i = static_cast<size_t>(end - s.c_str());
            while (i < s.size() && (s[i] == 'f' || s[i] == 'F' || s[i] == 'h' || s[i] == 'u'
                                    || s[i] == 'U' || s[i] == 'L'))
                ++i;
            return v;
        }
        size_t b = i;
        while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_')) ++i;
        const string word = lower(s.substr(b, i - b));
        if (word == "true") return 1;
        if (word == "false") return 0;
        bad = true;
        return 0;
    }
    double product()
    {
        double v = primary();
        for (;;)
        {
            ws();
            if (i < s.size() && s[i] == '*') { ++i; v *= primary(); }
            else if (i < s.size() && s[i] == '/') { ++i; v /= primary(); }
            else return v;
        }
    }
    double sum()
    {
        double v = product();
        for (;;)
        {
            ws();
            if (i < s.size() && s[i] == '+') { ++i; v += product(); }
            else if (i < s.size() && s[i] == '-') { ++i; v -= product(); }
            else return v;
        }
    }
    double shift()
    {
        double v = sum();
        for (;;)
        {
            ws();
            if (i + 1 < s.size() && s[i] == '<' && s[i + 1] == '<')
            { i += 2; v = static_cast<double>(static_cast<long long>(v) << static_cast<long long>(sum())); }
            else if (i + 1 < s.size() && s[i] == '>' && s[i + 1] == '>')
            { i += 2; v = static_cast<double>(static_cast<long long>(v) >> static_cast<long long>(sum())); }
            else return v;
        }
    }
    double expression()
    {
        double v = shift();
        for (;;)
        {
            ws();
            if (i < s.size() && s[i] == '|') { ++i; v = static_cast<double>(static_cast<long long>(v) | static_cast<long long>(shift())); }
            else if (i < s.size() && s[i] == '&') { ++i; v = static_cast<double>(static_cast<long long>(v) & static_cast<long long>(shift())); }
            else return v;
        }
    }
};

bool evaluate(const string& text, double* out)
{
    Evaluator e(text);
    *out = e.expression();
    e.ws();
    return !e.bad && e.i == text.size();
}

// Collects every number of an initializer: `{1, 2}`, `float4(1, 2, 3, 4)` or a nested mix.
std::vector<double> numberList(const string& text)
{
    std::vector<double> out;
    string item;
    const auto flush = [&]() {
        double v;
        size_t a = item.find_first_not_of(" \t\r\n");
        if (a != string::npos)
        {
            size_t b = item.find_last_not_of(" \t\r\n");
            if (evaluate(item.substr(a, b - a + 1), &v))
                out.push_back(v);
            else
                out.push_back(0);
        }
        item.clear();
    };
    for (size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '{' || c == '}' || c == ',')
            flush();
        else if (c == '(' )
        {
            // A constructor: what preceded the bracket is a type name, not a value.
            size_t e = item.size();
            while (e > 0 && std::isspace(static_cast<unsigned char>(item[e - 1]))) --e;
            if (e > 0 && (std::isalnum(static_cast<unsigned char>(item[e - 1])) || item[e - 1] == '_'))
                item.clear();
            else
                item.push_back(c);
        }
        else if (c == ')')
        {
            if (std::count(item.begin(), item.end(), '(') > std::count(item.begin(), item.end(), ')'))
                item.push_back(c);
            else
                flush();
        }
        else
            item.push_back(c);
    }
    flush();
    return out;
}

// ----- state tables -----------------------------------------------------------------------------

typedef std::map<string, unsigned> Table;

const Table kCmp = {{"never",1},{"less",2},{"equal",3},{"lessequal",4},{"greater",5},{"notequal",6},
                    {"greaterequal",7},{"always",8}};
const Table kBlend = {{"zero",1},{"one",2},{"srccolor",3},{"invsrccolor",4},{"srcalpha",5},
                      {"invsrcalpha",6},{"destalpha",7},{"invdestalpha",8},{"destcolor",9},
                      {"invdestcolor",10},{"srcalphasat",11},{"bothsrcalpha",12},
                      {"bothinvsrcalpha",13},{"blendfactor",14},{"invblendfactor",15}};
const Table kCull = {{"none",1},{"cw",2},{"ccw",3}};
const Table kFill = {{"point",1},{"wireframe",2},{"solid",3}};
const Table kShade = {{"flat",1},{"gouraud",2},{"phong",3}};
const Table kStencil = {{"keep",1},{"zero",2},{"replace",3},{"incrsat",4},{"decrsat",5},
                        {"invert",6},{"incr",7},{"decr",8}};
const Table kBlendOp = {{"add",1},{"subtract",2},{"revsubtract",3},{"min",4},{"max",5}};
const Table kFilter = {{"none",0},{"point",1},{"linear",2},{"anisotropic",3},{"pyramidalquad",6},
                       {"gaussianquad",7}};
const Table kAddress = {{"wrap",1},{"mirror",2},{"clamp",3},{"border",4},{"mirroronce",5}};
const Table kTop = {{"disable",1},{"selectarg1",2},{"selectarg2",3},{"modulate",4},{"modulate2x",5},
                    {"modulate4x",6},{"add",7},{"addsigned",8},{"addsigned2x",9},{"subtract",10},
                    {"addsmooth",11},{"blenddiffusealpha",12},{"blendtexturealpha",13},
                    {"blendfactoralpha",14},{"blendtexturealphapm",15},{"blendcurrentalpha",16},
                    {"premodulate",17},{"modulatealpha_addcolor",18},{"modulatecolor_addalpha",19},
                    {"modulateinvalpha_addcolor",20},{"modulateinvcolor_addalpha",21},
                    {"bumpenvmap",22},{"bumpenvmapluminance",23},{"dotproduct3",24},
                    {"multiplyadd",25},{"lerp",26}};
const Table kArg = {{"diffuse",0},{"current",1},{"texture",2},{"tfactor",3},{"specular",4},
                    {"temp",5},{"constant",6},{"complement",0x10},{"alphareplicate",0x20}};
const Table kTtff = {{"disable",0},{"count1",1},{"count2",2},{"count3",3},{"count4",4},
                     {"projected",256}};
const Table kWrite = {{"red",1},{"green",2},{"blue",4},{"alpha",8}};
const Table kFog = {{"none",0},{"exp",1},{"exp2",2},{"linear",3}};
const Table kNone;

struct StateInfo
{
    int kind;       // 0 render state, 1 texture stage state, 2 sampler state
    unsigned id;
    const Table* values;
    bool isFloat;
};

const std::map<string, StateInfo>& stateTable()
{
    static const std::map<string, StateInfo> table = {
        {"zenable",{0,7,&kNone,false}},{"fillmode",{0,8,&kFill,false}},{"shademode",{0,9,&kShade,false}},
        {"zwriteenable",{0,14,&kNone,false}},{"alphatestenable",{0,15,&kNone,false}},
        {"lastpixel",{0,16,&kNone,false}},{"srcblend",{0,19,&kBlend,false}},{"destblend",{0,20,&kBlend,false}},
        {"cullmode",{0,22,&kCull,false}},{"zfunc",{0,23,&kCmp,false}},{"alpharef",{0,24,&kNone,false}},
        {"alphafunc",{0,25,&kCmp,false}},{"ditherenable",{0,26,&kNone,false}},
        {"alphablendenable",{0,27,&kNone,false}},{"fogenable",{0,28,&kNone,false}},
        {"specularenable",{0,29,&kNone,false}},{"fogcolor",{0,34,&kNone,false}},
        {"fogtablemode",{0,35,&kFog,false}},{"fogstart",{0,36,&kNone,true}},{"fogend",{0,37,&kNone,true}},
        {"fogdensity",{0,38,&kNone,true}},{"rangefogenable",{0,48,&kNone,false}},
        {"stencilenable",{0,52,&kNone,false}},{"stencilfail",{0,53,&kStencil,false}},
        {"stencilzfail",{0,54,&kStencil,false}},{"stencilpass",{0,55,&kStencil,false}},
        {"stencilfunc",{0,56,&kCmp,false}},{"stencilref",{0,57,&kNone,false}},
        {"stencilmask",{0,58,&kNone,false}},{"stencilwritemask",{0,59,&kNone,false}},
        {"texturefactor",{0,60,&kNone,false}},{"clipping",{0,136,&kNone,false}},
        {"lighting",{0,137,&kNone,false}},{"ambient",{0,139,&kNone,false}},
        {"fogvertexmode",{0,140,&kFog,false}},{"colorvertex",{0,141,&kNone,false}},
        {"normalizenormals",{0,143,&kNone,false}},{"clipplaneenable",{0,152,&kNone,false}},
        {"pointsize",{0,154,&kNone,true}},{"pointsize_min",{0,155,&kNone,true}},
        {"pointspriteenable",{0,156,&kNone,false}},{"pointscaleenable",{0,157,&kNone,false}},
        {"pointscale_a",{0,158,&kNone,true}},{"pointscale_b",{0,159,&kNone,true}},
        {"pointscale_c",{0,160,&kNone,true}},{"multisampleantialias",{0,161,&kNone,false}},
        {"multisamplemask",{0,162,&kNone,false}},{"pointsize_max",{0,166,&kNone,true}},
        {"colorwriteenable",{0,168,&kWrite,false}},{"blendop",{0,171,&kBlendOp,false}},
        {"scissortestenable",{0,174,&kNone,false}},{"slopescaledepthbias",{0,175,&kNone,true}},
        {"antialiasedlineenable",{0,176,&kNone,false}},{"twosidedstencilmode",{0,185,&kNone,false}},
        {"ccw_stencilfail",{0,186,&kStencil,false}},{"ccw_stencilzfail",{0,187,&kStencil,false}},
        {"ccw_stencilpass",{0,188,&kStencil,false}},{"ccw_stencilfunc",{0,189,&kCmp,false}},
        {"colorwriteenable1",{0,190,&kWrite,false}},{"colorwriteenable2",{0,191,&kWrite,false}},
        {"colorwriteenable3",{0,192,&kWrite,false}},{"blendfactor",{0,193,&kNone,false}},
        {"srgbwriteenable",{0,194,&kNone,false}},{"depthbias",{0,195,&kNone,true}},
        {"separatealphablendenable",{0,206,&kNone,false}},{"srcblendalpha",{0,207,&kBlend,false}},
        {"destblendalpha",{0,208,&kBlend,false}},{"blendopalpha",{0,209,&kBlendOp,false}},
        {"colorop",{1,1,&kTop,false}},{"colorarg1",{1,2,&kArg,false}},{"colorarg2",{1,3,&kArg,false}},
        {"alphaop",{1,4,&kTop,false}},{"alphaarg1",{1,5,&kArg,false}},{"alphaarg2",{1,6,&kArg,false}},
        {"texcoordindex",{1,11,&kNone,false}},{"texturetransformflags",{1,24,&kTtff,false}},
        {"colorarg0",{1,26,&kArg,false}},{"alphaarg0",{1,27,&kArg,false}},{"resultarg",{1,28,&kArg,false}},
        {"addressu",{2,1,&kAddress,false}},{"addressv",{2,2,&kAddress,false}},
        {"addressw",{2,3,&kAddress,false}},{"bordercolor",{2,4,&kNone,false}},
        {"magfilter",{2,5,&kFilter,false}},{"minfilter",{2,6,&kFilter,false}},
        {"mipfilter",{2,7,&kFilter,false}},{"mipmaplodbias",{2,8,&kNone,true}},
        {"maxmiplevel",{2,9,&kNone,false}},{"maxanisotropy",{2,10,&kNone,false}},
        {"srgbtexture",{2,11,&kNone,false}},
    };
    return table;
}

// ----- the effect -------------------------------------------------------------------------------

class Effect;
void* const* effectVtable();

class Effect
{
public:
    void* const* vtable;
    int   refs = 1;
    IDirect3DDevice9* device;
    void* stateManager = nullptr;
    string path, body;
    std::vector<std::unique_ptr<Param>> params;
    std::vector<std::unique_ptr<Technique>> techniques;
    std::vector<std::unique_ptr<Shader>> shaders;
    std::map<string, Function> functions;
    std::map<string, std::vector<Shader*>> shaderVariables;
    std::map<string, Param*> byName;
    std::set<const void*> handles;
    Technique* current = nullptr;
    Pass* activePass = nullptr;
    std::set<string> warned;

    explicit Effect(IDirect3DDevice9* d) : vtable(effectVtable()), device(d) {}

    Param* param(const void* handle)
    {
        if (!handle) return nullptr;
        if (handles.count(handle)) return static_cast<Param*>(const_cast<void*>(handle));
        auto it = byName.find(static_cast<const char*>(handle));
        return it == byName.end() ? nullptr : it->second;
    }

    void warnOnce(const string& text)
    {
        if (warned.insert(text).second)
        {
            std::fprintf(stderr, "[effect] %s: %s\n", path.c_str(), text.c_str());
            std::fflush(stderr);
        }
    }

    bool build(Shader* shader);
    bool buildPass(Pass* pass);
    Shader* resolve(const ShaderRef& ref);
    void applyPass(Pass* pass);
    void uploadConstants(Pass* pass);
    void uploadOne(Shader* shader);
    void applySampler(Param* sampler, unsigned stage);
    bool stateValue(const StateInfo& info, const string& value, unsigned* out);

    void setRenderState(unsigned id, unsigned value);
    void setStageState(unsigned stage, unsigned id, unsigned value);
    void setSamplerState(unsigned stage, unsigned id, unsigned value);
    void setTexture(unsigned stage, IDirect3DBaseTexture9* texture);
    void setShader(bool pixel, void* object);
    void setConstantsF(bool pixel, unsigned reg, const float* data, unsigned count);
    void setConstantsI(bool pixel, unsigned reg, const int* data, unsigned count);
    void setConstantsB(bool pixel, unsigned reg, const int* data, unsigned count);
};

// ----- parsing ----------------------------------------------------------------------------------

class Parser
{
public:
    Parser(Effect& effect, const string& source) : fx(effect), s(source) {}
    void run();

private:
    Effect& fx;
    const string& s;
    size_t i = 0;
    string code;
    std::map<string, std::vector<std::pair<std::pair<unsigned, unsigned>, string>>> structs;

    void ws()
    {
        for (;;)
        {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            if (i < s.size() && s[i] == '#')
            {
                while (i < s.size() && s[i] != '\n') ++i;
                continue;
            }
            return;
        }
    }
    char peek() { ws(); return i < s.size() ? s[i] : '\0'; }
    bool eat(char c) { if (peek() == c) { ++i; return true; } return false; }
    string ident()
    {
        ws();
        size_t b = i;
        while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_')) ++i;
        return s.substr(b, i - b);
    }
    // Index just past the bracket matching the one at `at`.
    size_t matching(size_t at, char open, char close)
    {
        int depth = 0;
        bool str = false;
        for (size_t p = at; p < s.size(); ++p)
        {
            if (s[p] == '"') str = !str;
            if (str) continue;
            if (s[p] == open) ++depth;
            else if (s[p] == close && --depth == 0) return p + 1;
        }
        return s.size();
    }
    // Text up to the next `;` outside brackets; leaves i past the `;`.
    string untilSemicolon()
    {
        size_t b = i;
        int depth = 0;
        bool str = false;
        for (; i < s.size(); ++i)
        {
            const char c = s[i];
            if (c == '"') str = !str;
            if (str) continue;
            if (c == '{' || c == '(') ++depth;
            else if (c == '}' || c == ')') --depth;
            else if (c == ';' && depth <= 0) break;
        }
        string out = s.substr(b, i - b);
        if (i < s.size()) ++i;
        return out;
    }
    string readType()
    {
        string type = ident();
        if (peek() == '<')
        {
            size_t e = matching(i, '<', '>');
            type += s.substr(i, e - i);
            i = e;
        }
        return type;
    }

    void describe(Param& p, const string& type);
    void annotations(std::vector<std::unique_ptr<Param>>& out);
    void setDefault(Param& p, string value);
    void declaration(const string& first);
    void technique();
    void pass(Technique& t);
    void passState(Pass& p);
    std::vector<StateAssign> samplerState();
    Shader* compileSpec(const string& value, bool pixel);
    void structure();
    Param* addParam(std::unique_ptr<Param> p);
};

void Parser::describe(Param& p, const string& rawType)
{
    string t = lower(rawType);
    const size_t bracket = t.find('<');
    if (bracket != string::npos)
    {
        const std::vector<double> n = numberList(t.substr(bracket + 1, t.size() - bracket - 2).substr(
            t.find(',', bracket) == string::npos ? 0 : t.find(',', bracket) - bracket));
        const string base = t.substr(0, bracket);
        if (base == "matrix")
        {
            p.rows = n.size() > 0 ? static_cast<unsigned>(n[0]) : 4;
            p.columns = n.size() > 1 ? static_cast<unsigned>(n[1]) : 4;
            p.cls = PC_MATRIX_COLUMNS;
        }
        else
        {
            p.columns = n.empty() ? 4 : static_cast<unsigned>(n[0]);
            p.cls = PC_VECTOR;
        }
        return;
    }
    if (t == "vector") { p.cls = PC_VECTOR; p.columns = 4; return; }
    if (t == "matrix") { p.cls = PC_MATRIX_COLUMNS; p.rows = p.columns = 4; return; }
    if (t == "string") { p.cls = PC_OBJECT; p.type = PT_STRING; return; }
    static const std::map<string, unsigned> objects = {
        {"texture",PT_TEXTURE},{"texture1d",PT_TEXTURE1D},{"texture2d",PT_TEXTURE2D},
        {"texture3d",PT_TEXTURE3D},{"texturecube",PT_TEXTURECUBE},{"sampler",PT_SAMPLER},
        {"sampler1d",PT_SAMPLER1D},{"sampler2d",PT_SAMPLER2D},{"sampler3d",PT_SAMPLER3D},
        {"samplercube",PT_SAMPLERCUBE}};
    auto object = objects.find(t);
    if (object != objects.end()) { p.cls = PC_OBJECT; p.type = object->second; return; }

    size_t digits = 0;
    while (digits < t.size() && !std::isdigit(static_cast<unsigned char>(t[digits]))) ++digits;
    const string base = t.substr(0, digits);
    if (base == "int" || base == "uint" || base == "dword") p.type = PT_INT;
    else if (base == "bool") p.type = PT_BOOL;
    else p.type = PT_FLOAT;
    if (digits < t.size())
    {
        unsigned a = t[digits] - '0';
        if (digits + 2 < t.size() && t[digits + 1] == 'x')
        {
            p.rows = a;
            p.columns = static_cast<unsigned>(t[digits + 2] - '0');
            p.cls = PC_MATRIX_COLUMNS;
        }
        else
        {
            p.columns = a;
            p.cls = PC_VECTOR;
        }
    }
}

void Parser::setDefault(Param& p, string value)
{
    if (p.type == PT_STRING)
    {
        const bool array = p.elements != 0;
        size_t at = value.find('"');
        while (at != string::npos)
        {
            const size_t end = value.find('"', at + 1);
            if (end == string::npos) break;
            const string text = value.substr(at + 1, end - at - 1);
            if (!array)
            {
                p.text = text;
                break;
            }
            std::unique_ptr<Param> element(new Param);
            element->cls = PC_OBJECT;
            element->type = PT_STRING;
            element->text = text;
            fx.handles.insert(element.get());
            p.children.push_back(std::move(element));
            at = value.find('"', end + 1);
        }
        if (array) p.elements = static_cast<unsigned>(p.children.size());
        return;
    }
    if (p.isObject()) return;
    const std::vector<double> numbers = numberList(value);
    if (p.elements == 0xFFFFFFFFu)
        p.elements = static_cast<unsigned>(numbers.size() / std::max(1u, p.rows * p.columns));
    p.data.assign(p.count() * 4, 0);
    for (size_t n = 0; n < numbers.size() && n < p.count(); ++n)
    {
        if (p.type == PT_FLOAT)
            reinterpret_cast<float*>(p.data.data())[n] = static_cast<float>(numbers[n]);
        else
            reinterpret_cast<int*>(p.data.data())[n] = static_cast<int>(static_cast<long long>(numbers[n]));
    }
}

void Parser::annotations(std::vector<std::unique_ptr<Param>>& out)
{
    if (peek() != '<') return;
    ++i;
    while (peek() != '>' && i < s.size())
    {
        const string type = readType();
        if (type.empty()) { ++i; continue; }
        std::unique_ptr<Param> a(new Param);
        describe(*a, type);
        a->name = ident();
        if (peek() == '[')
        {
            size_t e = matching(i, '[', ']');
            double n = 0;
            string inner = s.substr(i + 1, e - i - 2);
            a->elements = evaluate(inner, &n) ? static_cast<unsigned>(n) : 0xFFFFFFFFu;
            i = e;
        }
        string value;
        if (eat('='))
            value = untilSemicolon();
        else
            eat(';');
        setDefault(*a, value);
        if (a->elements == 0xFFFFFFFFu) a->elements = 0;
        fx.handles.insert(a.get());
        out.push_back(std::move(a));
    }
    eat('>');
}

Param* Parser::addParam(std::unique_ptr<Param> p)
{
    Param* const raw = p.get();
    if (raw->data.empty() && !raw->isObject())
    {
        if (raw->elements == 0xFFFFFFFFu) raw->elements = 0;
        raw->data.assign(raw->count() * 4, 0);
    }
    fx.handles.insert(raw);
    fx.byName[raw->name] = raw;
    fx.params.push_back(std::move(p));
    return raw;
}

std::vector<StateAssign> Parser::samplerState()
{
    std::vector<StateAssign> out;
    if (!eat('{')) return out;
    while (peek() != '}' && i < s.size())
    {
        StateAssign state;
        state.name = lower(ident());
        state.index = 0;
        if (!eat('=')) { ++i; continue; }
        state.value = untilSemicolon();
        out.push_back(std::move(state));
    }
    eat('}');
    return out;
}

Shader* Parser::compileSpec(const string& value, bool pixel)
{
    Shader* const shader = new Shader;
    fx.shaders.emplace_back(shader);
    shader->pixel = pixel;
    size_t a = value.find_first_not_of(" \t\r\n{");
    const string word = lower(value.substr(a, 7));
    if (word.compare(0, 3, "asm") == 0)
    {
        size_t b = value.find('{', a);
        size_t e = value.rfind('}');
        shader->assembly = (b != string::npos && e > b) ? value.substr(b + 1, e - b - 1) : string();
        if (shader->assembly.empty()) shader->assembly = " ";
        return shader;
    }
    // compile <profile> <entry> ( args )
    Evaluator skip(value);
    size_t p = a + 7;
    const auto token = [&]() {
        while (p < value.size() && std::isspace(static_cast<unsigned char>(value[p]))) ++p;
        size_t b = p;
        while (p < value.size() && (std::isalnum(static_cast<unsigned char>(value[p])) || value[p] == '_')) ++p;
        return value.substr(b, p - b);
    };
    shader->profile = token();
    shader->entry = token();
    size_t open = value.find('(', p);
    size_t close = value.rfind(')');
    if (open != string::npos && close > open)
        shader->args = value.substr(open + 1, close - open - 1);
    return shader;
}

void Parser::passState(Pass& p)
{
    StateAssign state;
    string name = ident();
    state.name = lower(name);
    state.index = 0;
    if (peek() == '[')
    {
        size_t e = matching(i, '[', ']');
        double n = 0;
        if (evaluate(s.substr(i + 1, e - i - 2), &n)) state.index = static_cast<int>(n);
        i = e;
    }
    if (!eat('=')) { ++i; return; }
    ws();
    state.value = untilSemicolon();
    size_t a = state.value.find_first_not_of(" \t\r\n");
    if (a == string::npos) return;
    state.value = state.value.substr(a, state.value.find_last_not_of(" \t\r\n") - a + 1);

    if (state.name == "vertexshader" || state.name == "pixelshader")
    {
        const bool pixel = state.name[0] == 'p';
        ShaderRef& ref = pixel ? p.pixel : p.vertex;
        ref.present = true;
        const string head = lower(state.value.substr(0, 7));
        if (head.compare(0, 7, "compile") == 0 || head.compare(0, 3, "asm") == 0)
            ref.shader = compileSpec(state.value, pixel);
        else if (lower(state.value) != "null")
            ref.expression = state.value;
        return;
    }
    p.states.push_back(std::move(state));
}

void Parser::pass(Technique& t)
{
    std::unique_ptr<Pass> p(new Pass);
    if (peek() != '{' && peek() != '<')
        p->name = ident();
    annotations(p->annotations);
    if (eat('{'))
    {
        while (peek() != '}' && i < s.size())
            passState(*p);
        eat('}');
    }
    fx.handles.insert(p.get());
    t.passes.push_back(std::move(p));
}

void Parser::technique()
{
    std::unique_ptr<Technique> t(new Technique);
    if (peek() != '{' && peek() != '<')
        t->name = ident();
    annotations(t->annotations);
    if (eat('{'))
    {
        while (peek() != '}' && i < s.size())
        {
            const size_t before = i;
            const string word = lower(ident());
            if (word == "pass")
                pass(*t);
            else if (i == before)
                ++i;
        }
        eat('}');
    }
    fx.handles.insert(t.get());
    fx.techniques.push_back(std::move(t));
}

void Parser::structure()
{
    const size_t start = i;
    ident();    // struct
    const string name = ident();
    if (peek() != '{')
    {
        i = start;
        untilSemicolon();
        return;
    }
    i = matching(i, '{', '}');
    if (peek() != ';')
    {
        // An instance declared with the struct stays verbatim.
        untilSemicolon();
        code.append(s, start, i - start);
        code += '\n';
        return;
    }
    ++i;
    code.append(s, start, i - start);
    code += '\n';
    structs[name];
}

void Parser::declaration(const string&)
{
    const size_t start = i;
    string modifiers;
    bool isStatic = false;
    string word;
    for (;;)
    {
        const size_t mark = i;
        word = ident();
        const string l = lower(word);
        if (l == "static" || l == "const" || l == "uniform" || l == "shared" || l == "extern"
            || l == "volatile" || l == "inline" || l == "row_major" || l == "column_major")
        {
            if (l == "static") isStatic = true;
            if (l != "shared" && l != "extern") modifiers += word + " ";
            continue;
        }
        i = mark;
        break;
    }
    if (word.empty()) { ++i; return; }
    const string type = readType();
    string name = ident();
    if (name.empty()) { untilSemicolon(); return; }

    if (peek() == '(')
    {
        const size_t open = i;
        const size_t close = matching(i, '(', ')');
        Function f;
        f.ret = modifiers + type;
        f.params = s.substr(open + 1, close - open - 2);
        i = close;
        if (peek() == ':')
        {
            ++i;
            f.semantic = ident();
        }
        if (peek() == ';') { ++i; code.append(s, start, i - start); code += '\n'; return; }
        if (peek() != '{') { untilSemicolon(); return; }
        i = matching(i, '{', '}');
        code.append(s, start, i - start);
        code += '\n';
        fx.functions.emplace(name, f);
        return;
    }

    const string lowType = lower(type);
    const bool shaderVariable = lowType == "vertexshader" || lowType == "pixelshader";

    for (;;)
    {
        std::unique_ptr<Param> p(new Param);
        p->name = name;
        describe(*p, type);
        string arrays;
        if (peek() == '[')
        {
            const size_t e = matching(i, '[', ']');
            arrays = s.substr(i, e - i);
            double n = 0;
            const string inner = s.substr(i + 1, e - i - 2);
            if (inner.find_first_not_of(" \t") == string::npos)
                p->elements = 0xFFFFFFFFu;
            else if (evaluate(inner, &n))
                p->elements = static_cast<unsigned>(n);
            i = e;
        }
        if (peek() == ':')
        {
            ++i;
            const string semantic = ident();
            if (lower(semantic) == "register" || lower(semantic) == "packoffset")
            {
                if (peek() == '(') i = matching(i, '(', ')');
            }
            else
                p->semantic = semantic;
        }
        annotations(p->annotations);

        string value;
        bool hasValue = false;
        if (peek() == '=')
        {
            ++i;
            hasValue = true;
            const size_t mark = i;
            const string head = lower(ident());
            if (head == "sampler_state")
                p->samplerStates = samplerState();
            else
            {
                i = mark;
                // Stop at the declarator's `,` or `;`, brackets respected.
                ws();
                const size_t b = i;
                int depth = 0;
                bool str = false;
                for (; i < s.size(); ++i)
                {
                    const char c = s[i];
                    if (c == '"') str = !str;
                    if (str) continue;
                    if (c == '{' || c == '(') ++depth;
                    else if (c == '}' || c == ')') --depth;
                    else if ((c == ';' || c == ',') && depth == 0) break;
                }
                value = s.substr(b, i - b);
            }
        }

        if (shaderVariable)
        {
            // `vertexShader v = compile ...;` or `vertexShader v[2] = { compile ..., compile ... };`
            std::vector<Shader*>& list = fx.shaderVariables[name];
            const bool pixel = lowType[0] == 'p';
            size_t pos = 0;
            const string low = lower(value);
            for (;;)
            {
                size_t at = low.find("compile", pos);
                size_t atAsm = low.find("asm", pos);
                if (at == string::npos && atAsm == string::npos) break;
                if (atAsm < at) { list.push_back(compileSpec(value.substr(atAsm), pixel)); break; }
                int depth = 0;
                size_t e = at;
                for (; e < value.size(); ++e)
                {
                    if (value[e] == '(') ++depth;
                    else if (value[e] == ')' && --depth == 0) { ++e; break; }
                }
                list.push_back(compileSpec(value.substr(at, e - at), pixel));
                pos = e;
            }
        }
        else if (isStatic)
        {
            code += modifiers + type + " " + name + arrays;
            if (hasValue) code += " = " + value;
            code += ";\n";
        }
        else
        {
            if (hasValue && p->samplerStates.empty()) setDefault(*p, value);
            const bool object = p->isObject();
            const bool texture = object && p->type >= PT_TEXTURE && p->type <= PT_TEXTURECUBE;
            if (!object || (!texture && p->type != PT_STRING))
            {
                string dims = arrays;
                if (p->elements != 0 && p->elements != 0xFFFFFFFFu)
                    dims = "[" + std::to_string(p->elements) + "]";
                code += modifiers + type + " " + name + dims + ";\n";
            }
            addParam(std::move(p));
        }

        if (peek() == ',')
        {
            ++i;
            name = ident();
            continue;
        }
        eat(';');
        return;
    }
}

void Parser::run()
{
    while (peek() != '\0')
    {
        const size_t before = i;
        const string word = ident();
        const string low = lower(word);
        if (low == "technique")
            technique();
        else if (low == "struct")
        {
            i = before;
            ws();
            structure();
        }
        else if (low == "typedef")
        {
            i = before;
            ws();
            const size_t b = i;
            untilSemicolon();
            code.append(s, b, i - b);
            code += '\n';
        }
        else if (word.empty())
        {
            if (peek() == ';') ++i; else if (peek() == '[')
                i = matching(i, '[', ']');
            else ++i;
        }
        else
        {
            i = before;
            ws();
            declaration(word);
        }
        if (i == before) ++i;
    }
    fx.body = std::move(code);
}

// ----- compiling --------------------------------------------------------------------------------

std::vector<ShaderConstant> readConstants(const unsigned* code, size_t words)
{
    std::vector<ShaderConstant> out;
    for (size_t w = 1; w < words;)
    {
        const unsigned token = code[w];
        if ((token & 0xFFFF) != 0xFFFE) break;
        const unsigned length = (token >> 16) & 0x7FFF;
        if (length >= 7 && code[w + 1] == 0x42415443u)     // 'CTAB'
        {
            const unsigned char* const table = reinterpret_cast<const unsigned char*>(code + w + 2);
            const auto u32 = [&](size_t at) { unsigned v; std::memcpy(&v, table + at, 4); return v; };
            const auto u16 = [&](size_t at) { unsigned short v; std::memcpy(&v, table + at, 2); return static_cast<unsigned>(v); };
            const unsigned count = u32(12);
            const unsigned info = u32(16);
            for (unsigned c = 0; c < count; ++c)
            {
                const size_t at = info + c * 20u;
                ShaderConstant k;
                k.name = reinterpret_cast<const char*>(table + u32(at));
                k.set = u16(at + 4);
                k.reg = u16(at + 6);
                k.count = u16(at + 8);
                const size_t type = u32(at + 12);
                k.cls = u16(type);
                k.rows = u16(type + 4);
                k.columns = u16(type + 6);
                out.push_back(std::move(k));
            }
            break;
        }
        w += length + 1;
    }
    return out;
}

bool compileHlsl(const string& code, const char* entry, const char* profile,
                 std::vector<unsigned>* out, string* message)
{
    const vkd3d_shader_compile_option options[] = {
        {VKD3D_SHADER_COMPILE_OPTION_API_VERSION, VKD3D_SHADER_API_VERSION_1_17},
        {VKD3D_SHADER_COMPILE_OPTION_BACKWARD_COMPATIBILITY,
         VKD3D_SHADER_COMPILE_OPTION_DOUBLE_AS_FLOAT_ALIAS
             | VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_ALLOW_HALF_GLOBALS},
    };
    vkd3d_shader_hlsl_source_info hlsl = {};
    hlsl.type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO;
    hlsl.entry_point = entry;
    hlsl.profile = profile;
    vkd3d_shader_compile_info info = {};
    info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
    info.next = &hlsl;
    info.source.code = code.data();
    info.source.size = code.size();
    info.source_type = VKD3D_SHADER_SOURCE_HLSL;
    info.target_type = VKD3D_SHADER_TARGET_D3D_BYTECODE;
    info.options = options;
    info.option_count = 2;
    info.log_level = VKD3D_SHADER_LOG_ERROR;
    vkd3d_shader_code result = {};
    char* messages = nullptr;
    const int rc = vkd3d_shader_compile(&info, &result, &messages);
    if (messages)
    {
        *message = messages;
        vkd3d_shader_free_messages(messages);
    }
    if (rc < 0) return false;
    out->resize(result.size / 4);
    std::memcpy(out->data(), result.code, result.size);
    vkd3d_shader_free_shader_code(&result);
    return true;
}

std::vector<string> splitTop(const string& text)
{
    std::vector<string> out;
    string item;
    int depth = 0;
    for (char c : text)
    {
        if (c == '(' || c == '{' || c == '[') ++depth;
        if (c == ')' || c == '}' || c == ']') --depth;
        if (c == ',' && depth == 0) { out.push_back(item); item.clear(); }
        else item.push_back(c);
    }
    if (item.find_first_not_of(" \t\r\n") != string::npos) out.push_back(item);
    return out;
}

string paramName(const string& parameter)
{
    string head = parameter.substr(0, parameter.find(':'));
    head = head.substr(0, head.find('='));
    size_t e = head.find_last_not_of(" \t\r\n");
    if (e == string::npos) return string();
    if (head[e] == ']')
    {
        e = head.rfind('[', e);
        e = head.find_last_not_of(" \t\r\n", e - 1);
    }
    size_t b = e;
    while (b > 0 && (std::isalnum(static_cast<unsigned char>(head[b - 1])) || head[b - 1] == '_')) --b;
    return head.substr(b, e - b + 1);
}

bool Effect::build(Shader* shader)
{
    if (shader->state) return shader->state > 0;
    shader->state = -1;
    if (!shader->assembly.empty())
    {
        std::vector<unsigned> tokens;
        string error;
        if (!portshim::assembleD3dShader(shader->assembly, &tokens, &error))
        {
            warnOnce("assembly shader refused: " + error);
            return false;
        }
        const long created = shader->pixel
            ? device->CreatePixelShader(tokens.data(), reinterpret_cast<IDirect3DPixelShader9**>(&shader->object))
            : device->CreateVertexShader(tokens.data(), reinterpret_cast<IDirect3DVertexShader9**>(&shader->object));
        if (created < 0)
        {
            warnOnce("the device refused an assembly shader: " + shader->assembly.substr(0, 200));
            return false;
        }
        shader->state = 1;
        return true;
    }

    string code = body;
    string entry = shader->entry;
    auto function = functions.find(entry);
    if (function == functions.end())
    {
        warnOnce("entry point " + entry + " is not defined");
        return false;
    }
    const std::vector<string> given = splitTop(shader->args);
    if (!given.empty())
    {
        std::vector<string> parameters = splitTop(function->second.params);
        std::vector<bool> bound(parameters.size(), false);
        unsigned left = static_cast<unsigned>(given.size());
        for (size_t p = 0; p < parameters.size() && left; ++p)
            if (lower(parameters[p]).find("uniform") != string::npos) { bound[p] = true; --left; }
        for (size_t p = parameters.size(); p-- > 0 && left;)
            if (!bound[p]) { bound[p] = true; --left; }
        string kept, call;
        size_t next = 0;
        for (size_t p = 0; p < parameters.size(); ++p)
        {
            if (!call.empty()) call += ", ";
            if (bound[p])
                call += given[next < given.size() ? next++ : 0];
            else
            {
                if (!kept.empty()) kept += ", ";
                kept += parameters[p];
                call += paramName(parameters[p]);
            }
        }
        const Function& f = function->second;
        const bool isVoid = lower(f.ret) == "void";
        code += f.ret + " bf2EffectEntry(" + kept + ")";
        if (!f.semantic.empty()) code += " : " + f.semantic;
        code += " { " + string(isVoid ? "" : "return ") + entry + "(" + call + "); }\n";
        entry = "bf2EffectEntry";
    }

    const string profile = lower(shader->profile);
    const char* const stage = shader->pixel ? "ps" : "vs";
    const bool model3 = profile.find("_3_") != string::npos;
    std::vector<unsigned> words;
    string error;
    bool ok = !model3 && compileHlsl(code, entry.c_str(), (string(stage) + "_2_0").c_str(), &words, &error);
    if (!ok)
    {
        string fallback;
        ok = compileHlsl(code, entry.c_str(), (string(stage) + "_3_0").c_str(), &words, &fallback);
        if (!ok)
        {
            if (error.empty()) error = fallback;
            warnOnce(shader->entry + " (" + shader->profile + "): " + error.substr(0, 400));
            return false;
        }
    }
    const long hr = shader->pixel
        ? device->CreatePixelShader(words.data(), reinterpret_cast<IDirect3DPixelShader9**>(&shader->object))
        : device->CreateVertexShader(words.data(), reinterpret_cast<IDirect3DVertexShader9**>(&shader->object));
    if (hr < 0)
    {
        warnOnce(shader->entry + ": the device refused the shader");
        return false;
    }
    shader->constants = readConstants(words.data(), words.size());
    shader->state = 1;
    return true;
}

Shader* Effect::resolve(const ShaderRef& ref)
{
    if (ref.shader || ref.expression.empty()) return ref.shader;
    string text = ref.expression;
    text.erase(std::remove_if(text.begin(), text.end(), [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) || c == '(' || c == ')' || c == '<' || c == '>'; }),
        text.end());
    string name = text;
    size_t index = 0;
    const size_t bracket = text.find('[');
    if (bracket != string::npos)
    {
        name = text.substr(0, bracket);
        const string inner = text.substr(bracket + 1, text.find(']') - bracket - 1);
        double n = 0;
        if (evaluate(inner, &n))
            index = static_cast<size_t>(n);
        else if (Param* p = param(inner.c_str()))
        {
            if (!p->data.empty())
                index = p->type == PT_FLOAT
                    ? static_cast<size_t>(*reinterpret_cast<const float*>(p->data.data()))
                    : static_cast<size_t>(*reinterpret_cast<const int*>(p->data.data()));
        }
    }
    auto it = shaderVariables.find(name);
    if (it == shaderVariables.end() || index >= it->second.size())
    {
        warnOnce("cannot resolve the shader expression " + ref.expression);
        return nullptr;
    }
    return it->second[index];
}

bool Effect::buildPass(Pass* pass)
{
    bool ok = true;
    if (pass->vertex.present)
    {
        Shader* const shader = resolve(pass->vertex);
        if (shader) ok = build(shader) && ok;
        else if (!pass->vertex.expression.empty()) ok = false;
    }
    if (pass->pixel.present)
    {
        Shader* const shader = resolve(pass->pixel);
        if (shader) ok = build(shader) && ok;
        else if (!pass->pixel.expression.empty()) ok = false;
    }
    return ok;
}

// ----- applying ---------------------------------------------------------------------------------

template <typename R, typename... A>
R managerCall(void* manager, unsigned slot, A... args)
{
    void** const vtable = *reinterpret_cast<void***>(manager);
    return reinterpret_cast<R (*)(void*, A...)>(vtable[slot])(manager, args...);
}

void Effect::setRenderState(unsigned id, unsigned value)
{
    if (stateManager) managerCall<long>(stateManager, 7, id, value);
    else device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(id), value);
}

void Effect::setStageState(unsigned stage, unsigned id, unsigned value)
{
    if (stateManager) managerCall<long>(stateManager, 9, stage, id, value);
    else device->SetTextureStageState(stage, static_cast<D3DTEXTURESTAGESTATETYPE>(id), value);
}

void Effect::setSamplerState(unsigned stage, unsigned id, unsigned value)
{
    if (stateManager) managerCall<long>(stateManager, 10, stage, id, value);
    else device->SetSamplerState(stage, static_cast<D3DSAMPLERSTATETYPE>(id), value);
}

void Effect::setTexture(unsigned stage, IDirect3DBaseTexture9* texture)
{
    if (stateManager) managerCall<long>(stateManager, 8, stage, texture);
    else device->SetTexture(stage, texture);
}

void Effect::setShader(bool pixel, void* object)
{
    if (stateManager) managerCall<long>(stateManager, pixel ? 17 : 13, object);
    else if (pixel) device->SetPixelShader(static_cast<IDirect3DPixelShader9*>(object));
    else device->SetVertexShader(static_cast<IDirect3DVertexShader9*>(object));
}

void Effect::setConstantsF(bool pixel, unsigned reg, const float* data, unsigned count)
{
    if (stateManager) managerCall<long>(stateManager, pixel ? 18 : 14, reg, data, count);
    else if (pixel) device->SetPixelShaderConstantF(reg, data, count);
    else device->SetVertexShaderConstantF(reg, data, count);
}

void Effect::setConstantsI(bool pixel, unsigned reg, const int* data, unsigned count)
{
    if (stateManager) managerCall<long>(stateManager, pixel ? 19 : 15, reg, data, count);
    else if (pixel) device->SetPixelShaderConstantI(reg, data, count);
    else device->SetVertexShaderConstantI(reg, data, count);
}

void Effect::setConstantsB(bool pixel, unsigned reg, const int* data, unsigned count)
{
    if (stateManager) managerCall<long>(stateManager, pixel ? 20 : 16, reg, data, count);
    else if (pixel) device->SetPixelShaderConstantB(reg, data, count);
    else device->SetVertexShaderConstantB(reg, data, count);
}

// Reads a state's value: an enum name of that state, a number, or a parameter.
bool Effect::stateValue(const StateInfo& info, const string& rawValue, unsigned* out)
{
    string text;
    for (char c : rawValue)
        if (!std::isspace(static_cast<unsigned char>(c)) && c != '<' && c != '>')
            text.push_back(c);
    while (text.size() > 1 && text.front() == '(' && text.back() == ')')
        text = text.substr(1, text.size() - 2);

    if (Param* p = param(text.c_str()))
    {
        if (p->data.empty()) return false;
        if (p->type == PT_FLOAT)
        {
            const float f = *reinterpret_cast<const float*>(p->data.data());
            if (info.isFloat) std::memcpy(out, &f, 4);
            else *out = static_cast<unsigned>(f);
        }
        else
            *out = *reinterpret_cast<const unsigned*>(p->data.data());
        return true;
    }

    unsigned combined = 0;
    bool matched = !text.empty();
    size_t start = 0;
    while (matched && start <= text.size())
    {
        size_t bar = text.find('|', start);
        if (bar == string::npos) bar = text.size();
        string word = lower(text.substr(start, bar - start));
        if (word.compare(0, 3, "d3d") == 0)
        {
            const size_t underscore = word.find('_');
            if (underscore != string::npos) word = word.substr(underscore + 1);
        }
        auto it = info.values->find(word);
        if (it == info.values->end()) matched = false;
        else combined |= it->second;
        start = bar + 1;
    }
    if (matched) { *out = combined; return true; }

    double number = 0;
    if (!evaluate(text, &number)) return false;
    if (info.isFloat)
    {
        const float f = static_cast<float>(number);
        std::memcpy(out, &f, 4);
    }
    else
        *out = static_cast<unsigned>(static_cast<long long>(number));
    return true;
}

void Effect::applySampler(Param* sampler, unsigned stage)
{
    for (const StateAssign& state : sampler->samplerStates)
    {
        if (state.name == "texture")
        {
            string text;
            for (char c : state.value)
                if (!std::isspace(static_cast<unsigned char>(c)) && c != '<' && c != '>' && c != '(' && c != ')')
                    text.push_back(c);
            Param* const texture = param(text.c_str());
            setTexture(stage, texture ? texture->texture : nullptr);
            continue;
        }
        auto info = stateTable().find(state.name);
        unsigned value = 0;
        if (info != stateTable().end() && info->second.kind == 2 && stateValue(info->second, state.value, &value))
            setSamplerState(stage, info->second.id, value);
        else
            warnOnce("sampler state not understood: " + state.name + " = " + state.value);
    }
}

void Effect::uploadOne(Shader* shader)
{
    float registers[4 * 256];
    for (const ShaderConstant& k : shader->constants)
    {
        Param* const p = param(k.name.c_str());
        if (!p)
        {
            warnOnce("constant " + k.name + " has no parameter");
            continue;
        }
        if (k.set == 3)     // sampler
        {
            for (unsigned r = 0; r < k.count; ++r)
                applySampler(p, shader->pixel ? k.reg + r : D3DVERTEXTEXTURESAMPLER0 + k.reg + r);
            continue;
        }
        if (p->data.empty()) continue;
        const unsigned count = std::min(k.count, 256u);
        const unsigned rows = p->rows, columns = p->columns;
        const unsigned total = p->count();
        const auto read = [&](unsigned index) -> float {
            if (index >= total) return 0.0f;
            if (p->type == PT_FLOAT) return reinterpret_cast<const float*>(p->data.data())[index];
            return static_cast<float>(reinterpret_cast<const int*>(p->data.data())[index]);
        };
        if (k.set == 0)     // bool
        {
            int values[256];
            for (unsigned r = 0; r < count; ++r) values[r] = read(r) != 0.0f;
            setConstantsB(shader->pixel, k.reg, values, count);
            continue;
        }
        std::memset(registers, 0, count * 16);
        const bool matrix = p->cls == PC_MATRIX_ROWS || p->cls == PC_MATRIX_COLUMNS;
        if (matrix)
        {
            const bool byColumn = k.cls == PC_MATRIX_COLUMNS;
            const unsigned perElement = byColumn ? columns : rows;
            const unsigned size = rows * columns;
            for (unsigned r = 0; r < count; ++r)
            {
                const unsigned element = r / perElement, line = r % perElement;
                for (unsigned c = 0; c < 4; ++c)
                {
                    if (byColumn && c < rows)
                        registers[r * 4 + c] = read(element * size + c * columns + line);
                    else if (!byColumn && c < columns)
                        registers[r * 4 + c] = read(element * size + line * columns + c);
                }
            }
        }
        else
        {
            for (unsigned r = 0; r < count; ++r)
                for (unsigned c = 0; c < columns && c < 4; ++c)
                    registers[r * 4 + c] = read(r * columns + c);
        }
        if (k.set == 1)     // int
        {
            int values[4 * 256];
            for (unsigned n = 0; n < count * 4; ++n) values[n] = static_cast<int>(registers[n]);
            setConstantsI(shader->pixel, k.reg, values, count);
        }
        else
            setConstantsF(shader->pixel, k.reg, registers, count);
    }
}

void Effect::uploadConstants(Pass* pass)
{
    if (pass->vertex.present)
        if (Shader* const shader = resolve(pass->vertex))
            if (shader->state > 0) uploadOne(shader);
    if (pass->pixel.present)
        if (Shader* const shader = resolve(pass->pixel))
            if (shader->state > 0) uploadOne(shader);
}

void Effect::applyPass(Pass* pass)
{
    buildPass(pass);
    for (const StateAssign& state : pass->states)
    {
        if (state.name == "sampler")
        {
            string text;
            for (char c : state.value)
                if (!std::isspace(static_cast<unsigned char>(c)) && c != '<' && c != '>' && c != '(' && c != ')')
                    text.push_back(c);
            if (Param* const sampler = param(text.c_str()))
                applySampler(sampler, static_cast<unsigned>(state.index));
            else
                warnOnce("sampler expression not understood: " + state.value);
            continue;
        }
        if (state.name == "texture")
        {
            string text;
            for (char c : state.value)
                if (!std::isspace(static_cast<unsigned char>(c)) && c != '<' && c != '>' && c != '(' && c != ')')
                    text.push_back(c);
            Param* const texture = param(text.c_str());
            setTexture(static_cast<unsigned>(state.index), texture ? texture->texture : nullptr);
            continue;
        }
        if (state.name == "pixelshaderconstantf" || state.name == "pixelshaderconstant"
            || state.name == "vertexshaderconstantf" || state.name == "vertexshaderconstant")
        {
            float values[4 * 8] = {};
            string text = state.value;
            text.erase(std::remove_if(text.begin(), text.end(), [](char c) { return c == '<' || c == '>'; }), text.end());
            size_t a = text.find_first_not_of(" \t(");
            size_t b = text.find_last_not_of(" \t)");
            const string bare = a == string::npos ? string() : text.substr(a, b - a + 1);
            unsigned registers = 1;
            if (Param* p = param(bare.c_str()))
            {
                const unsigned count = std::min(p->count(), 32u);
                if (p->type == PT_FLOAT && !p->data.empty())
                    std::memcpy(values, p->data.data(), count * 4);
                registers = (count + 3) / 4;
            }
            else
            {
                const std::vector<double> numbers = numberList(text);
                for (size_t n = 0; n < numbers.size() && n < 32; ++n)
                    values[n] = static_cast<float>(numbers[n]);
                registers = (static_cast<unsigned>(std::min<size_t>(numbers.size(), 32)) + 3) / 4;
            }
            setConstantsF(state.name[0] == 'p', static_cast<unsigned>(state.index), values, std::max(1u, registers));
            continue;
        }
        auto info = stateTable().find(state.name);
        if (info == stateTable().end())
        {
            warnOnce("unknown pass state: " + state.name);
            continue;
        }
        unsigned value = 0;
        if (!stateValue(info->second, state.value, &value))
        {
            warnOnce("state value not understood: " + state.name + " = " + state.value);
            continue;
        }
        const unsigned stage = static_cast<unsigned>(state.index);
        if (info->second.kind == 0) setRenderState(info->second.id, value);
        else if (info->second.kind == 1) setStageState(stage, info->second.id, value);
        else setSamplerState(stage, info->second.id, value);
    }
    if (pass->vertex.present)
    {
        Shader* const shader = resolve(pass->vertex);
        setShader(false, shader && shader->state > 0 ? shader->object : nullptr);
    }
    if (pass->pixel.present)
    {
        Shader* const shader = resolve(pass->pixel);
        setShader(true, shader && shader->state > 0 ? shader->object : nullptr);
    }
    uploadConstants(pass);
}

// ----- the vtable -------------------------------------------------------------------------------

Effect* self(void* p) { return static_cast<Effect*>(p); }

struct TechniqueDesc { const char* name; unsigned passes, annotations; };
struct ParameterDesc
{
    const char* name; const char* semantic;
    unsigned cls, type, rows, columns, elements, annotations, structMembers, flags, bytes;
};

unsigned long fxAddRef(void* e) { return static_cast<unsigned long>(++self(e)->refs); }
unsigned long fxRelease(void* e)
{
    Effect* const fx = self(e);
    const int left = --fx->refs;
    if (left == 0)
    {
        for (auto& p : fx->params)
            if (p->texture) p->texture->Release();
        for (auto& s : fx->shaders)
            if (s->object) static_cast<IUnknown*>(s->object)->Release();
        delete fx;
    }
    return static_cast<unsigned long>(left);
}

long fxGetParameterDesc(void* e, void* handle, ParameterDesc* out)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    out->name = p->name.c_str();
    out->semantic = p->semantic.empty() ? nullptr : p->semantic.c_str();
    // The renderer only handles row-class matrices; registers follow the constant table regardless.
    out->cls = p->cls == PC_MATRIX_COLUMNS ? static_cast<unsigned>(PC_MATRIX_ROWS) : p->cls;
    out->type = p->type;
    out->rows = p->rows;
    out->columns = p->columns;
    out->elements = p->elements == 0xFFFFFFFFu ? 0 : p->elements;
    out->annotations = static_cast<unsigned>(p->annotations.size());
    out->structMembers = 0;
    out->flags = 0;
    out->bytes = p->isObject() ? 4u : p->count() * 4;
    return kOk;
}

long fxGetTechniqueDesc(void* e, void* handle, TechniqueDesc* out)
{
    Effect* const fx = self(e);
    Technique* technique = nullptr;
    if (fx->handles.count(handle)) technique = static_cast<Technique*>(handle);
    else
        for (auto& t : fx->techniques)
            if (handle && t->name == static_cast<const char*>(handle)) technique = t.get();
    if (!technique) return kFail;
    out->name = technique->name.c_str();
    out->passes = static_cast<unsigned>(technique->passes.size());
    out->annotations = static_cast<unsigned>(technique->annotations.size());
    return kOk;
}

void* fxGetParameter(void* e, void* parent, unsigned index)
{
    Effect* const fx = self(e);
    if (parent) return nullptr;
    return index < fx->params.size() ? fx->params[index].get() : nullptr;
}

void* fxGetParameterByName(void* e, void* parent, const char* name)
{
    Effect* const fx = self(e);
    if (parent) return nullptr;
    auto it = fx->byName.find(name);
    return it == fx->byName.end() ? nullptr : it->second;
}

void* fxGetParameterElement(void* e, void* handle, unsigned index)
{
    Param* const p = self(e)->param(handle);
    return p && index < p->children.size() ? p->children[index].get() : nullptr;
}

long fxGetString(void* e, void* handle, const char** out)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    *out = p->text.c_str();
    return kOk;
}

void* fxGetTechnique(void* e, unsigned index)
{
    Effect* const fx = self(e);
    return index < fx->techniques.size() ? fx->techniques[index].get() : nullptr;
}

void* fxGetTechniqueByName(void* e, const char* name)
{
    for (auto& t : self(e)->techniques)
        if (t->name == name) return t.get();
    return nullptr;
}

void* fxGetPass(void* e, void* technique, unsigned index)
{
    Technique* const t = static_cast<Technique*>(technique);
    (void)e;
    return t && index < t->passes.size() ? t->passes[index].get() : nullptr;
}

void* fxGetAnnotationByName(void* e, void* handle, const char* name)
{
    Effect* const fx = self(e);
    if (!handle || !fx->handles.count(handle)) return nullptr;
    // Each annotation owner's list is searched by the owner's kind.
    for (auto& t : fx->techniques)
    {
        if (t.get() == handle)
        {
            for (auto& a : t->annotations)
                if (a->name == name) return a.get();
            return nullptr;
        }
        for (auto& p : t->passes)
            if (p.get() == handle)
            {
                for (auto& a : p->annotations)
                    if (a->name == name) return a.get();
                return nullptr;
            }
    }
    for (auto& a : static_cast<Param*>(handle)->annotations)
        if (a->name == name) return a.get();
    return nullptr;
}

void storeFloats(Param* p, const float* data, unsigned count)
{
    count = std::min(count, p->count());
    for (unsigned n = 0; n < count; ++n)
    {
        if (p->type == PT_FLOAT) reinterpret_cast<float*>(p->data.data())[n] = data[n];
        else if (p->type == PT_BOOL) reinterpret_cast<int*>(p->data.data())[n] = data[n] != 0.0f;
        else reinterpret_cast<int*>(p->data.data())[n] = static_cast<int>(data[n]);
    }
}

void storeInts(Param* p, const int* data, unsigned count)
{
    count = std::min(count, p->count());
    for (unsigned n = 0; n < count; ++n)
    {
        if (p->type == PT_FLOAT) reinterpret_cast<float*>(p->data.data())[n] = static_cast<float>(data[n]);
        else if (p->type == PT_BOOL) reinterpret_cast<int*>(p->data.data())[n] = data[n] != 0;
        else reinterpret_cast<int*>(p->data.data())[n] = data[n];
    }
}

// Stores `count` 4x4 row-major matrices into the parameter's tight rows x columns storage.
void storeMatrices(Param* p, const float* const* matrices, unsigned count)
{
    const unsigned size = p->rows * p->columns;
    const unsigned elements = p->elements ? p->elements : 1;
    if (p->data.empty()) return;
    float* const out = reinterpret_cast<float*>(p->data.data());
    for (unsigned m = 0; m < count && m < elements; ++m)
        for (unsigned r = 0; r < p->rows && r < 4; ++r)
            for (unsigned c = 0; c < p->columns && c < 4; ++c)
                out[m * size + r * p->columns + c] = matrices[m][r * 4 + c];
}

long fxSetValue(void* e, void* handle, const void* data, unsigned bytes)
{
    Param* const p = self(e)->param(handle);
    if (!p || p->data.empty()) return kFail;
    std::memcpy(p->data.data(), data, std::min<size_t>(bytes, p->data.size()));
    return kOk;
}

long fxSetBool(void* e, void* handle, int value)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    storeInts(p, &value, 1);
    return kOk;
}

long fxGetInt(void* e, void* handle, int* out)
{
    Param* const p = self(e)->param(handle);
    if (!p || p->data.empty()) return kFail;
    *out = p->type == PT_FLOAT ? static_cast<int>(*reinterpret_cast<const float*>(p->data.data()))
                               : *reinterpret_cast<const int*>(p->data.data());
    return kOk;
}

long fxGetBool(void* e, void* handle, int* out)
{
    int value = 0;
    const long hr = fxGetInt(e, handle, &value);
    if (hr >= 0) *out = value != 0;
    return hr;
}

long fxGetIntArray(void* e, void* handle, int* out, unsigned count)
{
    Param* const p = self(e)->param(handle);
    if (!p || p->data.empty()) return kFail;
    count = std::min(count, p->count());
    for (unsigned n = 0; n < count; ++n)
        out[n] = p->type == PT_FLOAT ? static_cast<int>(reinterpret_cast<const float*>(p->data.data())[n])
                                     : reinterpret_cast<const int*>(p->data.data())[n];
    return kOk;
}

long fxSetBoolArray(void* e, void* handle, const int* data, unsigned count)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    storeInts(p, data, count);
    return kOk;
}

long fxSetFloat(void* e, void* handle, float value)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    storeFloats(p, &value, 1);
    return kOk;
}

long fxSetFloatArray(void* e, void* handle, const float* data, unsigned count)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    storeFloats(p, data, count);
    return kOk;
}

long fxSetVector(void* e, void* handle, const float* data)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    storeFloats(p, data, p->rows * p->columns);
    return kOk;
}

long fxSetVectorArray(void* e, void* handle, const float* data, unsigned count)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    // Each element arrives as four floats, whatever the parameter's width.
    const unsigned width = p->columns;
    std::vector<float> packed;
    packed.reserve(count * width);
    for (unsigned n = 0; n < count; ++n)
        for (unsigned c = 0; c < width && c < 4; ++c)
            packed.push_back(data[n * 4 + c]);
    storeFloats(p, packed.data(), static_cast<unsigned>(packed.size()));
    return kOk;
}

long fxSetMatrix(void* e, void* handle, const float* matrix)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    storeMatrices(p, &matrix, 1);
    return kOk;
}

long fxSetMatrixArray(void* e, void* handle, const float* data, unsigned count)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    std::vector<const float*> pointers(count);
    for (unsigned n = 0; n < count; ++n) pointers[n] = data + n * 16;
    storeMatrices(p, pointers.data(), count);
    return kOk;
}

long fxSetMatrixPointerArray(void* e, void* handle, const float* const* matrices, unsigned count)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    storeMatrices(p, matrices, count);
    return kOk;
}

long fxSetTexture(void* e, void* handle, IDirect3DBaseTexture9* texture)
{
    Param* const p = self(e)->param(handle);
    if (!p) return kFail;
    if (texture) texture->AddRef();
    if (p->texture) p->texture->Release();
    p->texture = texture;
    return kOk;
}

long fxSetArrayRange(void*, void*, unsigned, unsigned) { return kOk; }

long fxSetTechnique(void* e, void* handle)
{
    Effect* const fx = self(e);
    for (auto& t : fx->techniques)
        if (t.get() == handle || (handle && !fx->handles.count(handle) && t->name == static_cast<const char*>(handle)))
        {
            fx->current = t.get();
            return kOk;
        }
    return kFail;
}

long fxValidateTechnique(void* e, void* handle)
{
    Effect* const fx = self(e);
    Technique* const t = static_cast<Technique*>(handle);
    if (!t) return kFail;
    bool ok = true;
    for (auto& pass : t->passes)
        ok = fx->buildPass(pass.get()) && ok;
    return ok ? kOk : kFail;
}

int fxIsParameterUsed(void* e, void* handle, void* technique)
{
    Effect* const fx = self(e);
    Param* const p = fx->param(handle);
    Technique* const t = static_cast<Technique*>(technique);
    if (!p || !t) return 0;
    // A sampler used through a texture counts for the texture too.
    const auto samplerUses = [&](const string& constant) {
        Param* const sampler = fx->param(constant.c_str());
        if (!sampler) return false;
        for (const StateAssign& state : sampler->samplerStates)
            if (state.name == "texture" && state.value.find(p->name) != string::npos) return true;
        return false;
    };
    for (auto& pass : t->passes)
    {
        fx->buildPass(pass.get());
        for (const StateAssign& state : pass->states)
            if (state.value.find(p->name) != string::npos) return 1;
        for (const ShaderRef* ref : {&pass->vertex, &pass->pixel})
        {
            Shader* const shader = ref->present ? fx->resolve(*ref) : nullptr;
            if (!shader) continue;
            for (const ShaderConstant& k : shader->constants)
                if (k.name == p->name || (k.set == 3 && samplerUses(k.name))) return 1;
        }
    }
    return 0;
}

long fxBegin(void* e, unsigned* passes, unsigned)
{
    Effect* const fx = self(e);
    if (!fx->current && !fx->techniques.empty()) fx->current = fx->techniques[0].get();
    if (passes) *passes = fx->current ? static_cast<unsigned>(fx->current->passes.size()) : 0;
    return fx->current ? kOk : kFail;
}

long fxBeginPass(void* e, unsigned index)
{
    Effect* const fx = self(e);
    if (!fx->current || index >= fx->current->passes.size()) return kFail;
    fx->activePass = fx->current->passes[index].get();
    fx->applyPass(fx->activePass);
    return kOk;
}

long fxCommitChanges(void* e)
{
    Effect* const fx = self(e);
    if (fx->activePass) fx->uploadConstants(fx->activePass);
    return kOk;
}

long fxEndPass(void* e) { self(e)->activePass = nullptr; return kOk; }
long fxEnd(void*) { return kOk; }
long fxOnDevice(void*) { return kOk; }

long fxSetStateManager(void* e, void* manager)
{
    Effect* const fx = self(e);
    if (manager) managerCall<unsigned long>(manager, 1);
    if (fx->stateManager) managerCall<unsigned long>(fx->stateManager, 2);
    fx->stateManager = manager;
    return kOk;
}

template <int Slot>
void fxMissing()
{
    std::fprintf(stderr, "[FRONTIER] ID3DXEffect slot +0x%X reached\n", Slot * 4);
    std::fflush(stderr);
    std::abort();
}

template <int... S> struct Slots {};
template <int N, int... S> struct MakeSlots : MakeSlots<N - 1, N - 1, S...> {};
template <int... S> struct MakeSlots<0, S...> { typedef Slots<S...> type; };

template <int... S>
void fillMissing(void** table, Slots<S...>)
{
    void* const entries[] = {reinterpret_cast<void*>(&fxMissing<S>)...};
    std::memcpy(table, entries, sizeof(entries));
}

void* const* effectVtable()
{
    static void* table[78];
    static bool ready = false;
    if (!ready)
    {
        fillMissing(table, MakeSlots<78>::type());
        const auto set = [](unsigned offset, void* entry) { table[offset / 4] = entry; };
        set(0x04, reinterpret_cast<void*>(&fxAddRef));
        set(0x08, reinterpret_cast<void*>(&fxRelease));
        set(0x10, reinterpret_cast<void*>(&fxGetParameterDesc));
        set(0x14, reinterpret_cast<void*>(&fxGetTechniqueDesc));
        set(0x20, reinterpret_cast<void*>(&fxGetParameter));
        set(0x24, reinterpret_cast<void*>(&fxGetParameterByName));
        set(0x2C, reinterpret_cast<void*>(&fxGetParameterElement));
        set(0xCC, reinterpret_cast<void*>(&fxGetString));
        set(0x30, reinterpret_cast<void*>(&fxGetTechnique));
        set(0x34, reinterpret_cast<void*>(&fxGetTechniqueByName));
        set(0x38, reinterpret_cast<void*>(&fxGetPass));
        set(0x4C, reinterpret_cast<void*>(&fxGetAnnotationByName));
        set(0x50, reinterpret_cast<void*>(&fxSetValue));
        set(0x58, reinterpret_cast<void*>(&fxSetBool));
        set(0x5C, reinterpret_cast<void*>(&fxGetBool));
        set(0x60, reinterpret_cast<void*>(&fxSetBoolArray));
        set(0x68, reinterpret_cast<void*>(&fxSetBool));
        set(0x6C, reinterpret_cast<void*>(&fxGetInt));
        set(0x70, reinterpret_cast<void*>(&fxSetBoolArray));
        set(0x74, reinterpret_cast<void*>(&fxGetIntArray));
        set(0x78, reinterpret_cast<void*>(&fxSetFloat));
        set(0x80, reinterpret_cast<void*>(&fxSetFloatArray));
        set(0x88, reinterpret_cast<void*>(&fxSetVector));
        set(0x90, reinterpret_cast<void*>(&fxSetVectorArray));
        set(0x98, reinterpret_cast<void*>(&fxSetMatrix));
        set(0xA0, reinterpret_cast<void*>(&fxSetMatrixArray));
        set(0xA8, reinterpret_cast<void*>(&fxSetMatrixPointerArray));
        set(0xD0, reinterpret_cast<void*>(&fxSetTexture));
        set(0xE0, reinterpret_cast<void*>(&fxSetArrayRange));
        set(0xE8, reinterpret_cast<void*>(&fxSetTechnique));
        set(0xF0, reinterpret_cast<void*>(&fxValidateTechnique));
        set(0xF8, reinterpret_cast<void*>(&fxIsParameterUsed));
        set(0xFC, reinterpret_cast<void*>(&fxBegin));
        set(0x100, reinterpret_cast<void*>(&fxBeginPass));
        set(0x104, reinterpret_cast<void*>(&fxCommitChanges));
        set(0x108, reinterpret_cast<void*>(&fxEndPass));
        set(0x10C, reinterpret_cast<void*>(&fxEnd));
        set(0x114, reinterpret_cast<void*>(&fxOnDevice));
        set(0x118, reinterpret_cast<void*>(&fxOnDevice));
        set(0x11C, reinterpret_cast<void*>(&fxSetStateManager));
        ready = true;
    }
    return table;
}

// ----- preprocessing ----------------------------------------------------------------------------

struct IncludeContext { void* include; };

typedef long (*IncludeOpenFn)(void*, unsigned, const char*, const void*, const void**, unsigned*);
typedef long (*IncludeCloseFn)(void*, const void*);

int openInclude(const char* filename, bool local, const char*, void* context, vkd3d_shader_code* out)
{
    IncludeContext* const ctx = static_cast<IncludeContext*>(context);
    if (!ctx->include) return VKD3D_ERROR;
    void** const vtable = *reinterpret_cast<void***>(ctx->include);
    const void* data = nullptr;
    unsigned bytes = 0;
    const long hr = reinterpret_cast<IncludeOpenFn>(vtable[0])(ctx->include, local ? 0 : 1, filename,
                                                               nullptr, &data, &bytes);
    if (hr < 0 || !data) return VKD3D_ERROR;
    // The copy keeps the text ours for vkd3d's lifetime; the host's buffer goes back at once.
    char* const copy = static_cast<char*>(std::malloc(bytes + 1));
    std::memcpy(copy, data, bytes);
    copy[bytes] = '\0';
    reinterpret_cast<IncludeCloseFn>(vtable[1])(ctx->include, data);
    out->code = copy;
    out->size = bytes;
    return VKD3D_OK;
}

void closeInclude(const vkd3d_shader_code* code, void*)
{
    std::free(const_cast<void*>(code->code));
}

} // namespace

struct PortableMacro { const char* name; const char* definition; };

extern "C" long D3DXCreateEffect(void* device, const void* source, unsigned int bytes,
                                 const PortableMacro* defines, void* include, unsigned long,
                                 void*, void** effect, void** errors)
{
    if (errors) *errors = nullptr;
    std::vector<vkd3d_shader_macro> macros;
    for (const PortableMacro* m = defines; m && m->name; ++m)
    {
        vkd3d_shader_macro macro;
        macro.name = m->name;
        macro.value = m->definition ? m->definition : "1";
        macros.push_back(macro);
    }

    IncludeContext context = {include};
    vkd3d_shader_preprocess_info preprocess = {};
    preprocess.type = VKD3D_SHADER_STRUCTURE_TYPE_PREPROCESS_INFO;
    preprocess.macros = macros.data();
    preprocess.macro_count = static_cast<unsigned>(macros.size());
    preprocess.pfn_open_include = openInclude;
    preprocess.pfn_close_include = closeInclude;
    preprocess.include_context = &context;

    vkd3d_shader_hlsl_source_info hlsl = {};
    hlsl.type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO;
    hlsl.next = &preprocess;
    hlsl.profile = "ps_2_0";

    vkd3d_shader_compile_info info = {};
    info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
    info.next = &hlsl;
    info.source.code = source;
    info.source.size = bytes;
    info.source_type = VKD3D_SHADER_SOURCE_HLSL;
    info.target_type = VKD3D_SHADER_TARGET_D3D_BYTECODE;
    info.log_level = VKD3D_SHADER_LOG_ERROR;

    vkd3d_shader_code text = {};
    char* messages = nullptr;
    const int rc = vkd3d_shader_preprocess(&info, &text, &messages);
    if (rc < 0)
    {
        std::fprintf(stderr, "[effect] preprocessing failed: %s\n", messages ? messages : "");
        std::fflush(stderr);
        vkd3d_shader_free_messages(messages);
        return kFail;
    }
    vkd3d_shader_free_messages(messages);

    Effect* const fx = new Effect(static_cast<IDirect3DDevice9*>(device));
    const string preprocessed(static_cast<const char*>(text.code), text.size);
    vkd3d_shader_free_shader_code(&text);
    Parser(*fx, preprocessed).run();
    std::fprintf(stderr, "[effect] parsed %u parameters, %u techniques, %u functions\n",
                 static_cast<unsigned>(fx->params.size()), static_cast<unsigned>(fx->techniques.size()),
                 static_cast<unsigned>(fx->functions.size()));
    std::fflush(stderr);
    *effect = fx;
    return kOk;
}

// ID3DXEffectCompiler stand-in: only its reflection is used, and that is the effect's own surface.
extern "C" long portableCreateEffectCompiler(const void* source, unsigned int bytes,
                                             const PortableMacro* defines, void* include,
                                             void** compiler, void** errors)
{
    return D3DXCreateEffect(nullptr, source, bytes, defines, include, 0, nullptr, compiler, errors);
}

extern "C" unsigned long D3DXGetShaderVersion(const unsigned long* function)
{
    return function ? *reinterpret_cast<const unsigned*>(function) : 0;
}
