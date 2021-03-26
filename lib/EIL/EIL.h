#ifndef EIL_h
#define EIL_h

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

using std::string;

#define STACK_SIZE 64
#define VAR_SIZE 64

class EIL
{
private:
    /********************Defintions**********************/
    bool executableOK = false;
    bool commandsDone = false;
    std::string loop;
    const char *testVar = "1621;|1721;1C1000;1821;1C1000;";
    //const char* ilTest = "0113;08187;0A10;0810;0A187;0810;02a;0113.2;08187.02;0A10;02b;0113.2;0C187.02;02c;012002.20;0Fb;02d;01TRUE;05FALSE;06TRUE;07FALSE;02e;0113;08187;0A10;0810;0A187;A01000;02f;";     //0113.2;0C187.02;02c;             //LD 13     ADD 5       MUL 10      ST a
    char *script;
    char **lines;
    uint16_t n_lines = 0;
    uint16_t programCounter = 0;

    struct stack
    {
        char mem[STACK_SIZE][VAR_SIZE]; //This is where the results are stored, the real stack
        uint16_t size;
        uint16_t maxSize;
    };

    struct functions
    {
        void (*functions[128])(char *);
        const char **name;
        uint8_t code[64];
        uint16_t size;
    };

    struct variables
    {
        char *(*externVar[128])(char *);
        uint8_t isExternal[VAR_SIZE];
        char runtimeVar[128][VAR_SIZE];
        char name[128][VAR_SIZE];
        uint16_t size;
    };

    stack il_stack;
    functions *il_functions;
    variables il_variables;

    /*******************Stack Prototypes************************/
    void createStack();
    void stackPush(char *data);
    char *stackPop();
    void freeStack();
    uint8_t isEmpty();
    uint8_t isFull();

    /**************Function Callback Prototypes*****************/

    void initFunctions();

    /******************Variables Prototypes*********************/

    void initVariables();
    char *fetchVariable(const char *arg, const char *op);
    void addVariable(const char *varname, const char *value);

    void changeVariable(const char *varname, const char *newVal);
    uint8_t isVariableAvailable(const char *varname);

    /******************Conversion & Checks**********************/
    uint8_t convertOPChartoHex(char *op);
    uint8_t isArgVar(char *arg, uint8_t op);

    /******************Runtime Prototypes************************/

    void splitAndAssignList(char *instructions, uint8_t shouldExecute);

    char *substring(char string[1024], uint8_t startIndex, uint8_t stopIndex);

public:
    void start();
    void reset();
    void initVM();
    void handleVM();
    void insertScript(const char *script);
    void execute(char **lines);
    void registerFunction(const char *name, uint8_t code, void (*func)(char *));
    void registerVariable(const char *name, char *(*func)(char *));
    char *getVariable(const char *varname);
    void removeChar(char *s, char c);
    void getParameters(char **str, char **params);
    uint8_t isArgFloat(char *arg, uint8_t op);
    uint8_t isArgInt(char *arg, uint8_t op);
    uint8_t isArgBool(char *arg, uint8_t op);
    uint8_t isArgNum(char *arg, uint8_t op);
};

#endif

/*



*/
