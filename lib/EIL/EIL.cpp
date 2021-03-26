#include "EIL.h"
#include <Arduino.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <iomanip>

/************OPERATORS USED FOR INTERPRETING*************/

#define LD 0x01    //Done
#define ST 0x02    //Done
#define S 0x03     //Done
#define R 0x04     //Done
#define AND 0x05   //Done
#define OR 0x06    //Done
#define XOR 0x07   //Done
#define ADD 0x08   //Done
#define SUB 0x09   //Done
#define MUL 0x0A   //Done
#define DIV 0x0B   //Done
#define GT 0x0C    //Done
#define GE 0x0D    //Done
#define EQ 0x0E    //Done
#define NE 0x0F    //Done
#define LE 0x10    //Done
#define LT 0x11    //Done
#define ANDN 0x12  //Done
#define ANDE 0x13  // AND(  //Done
#define ANDNE 0x14 // ANDN( //Done
#define ORN 0x15   //Done
#define ORE 0x16   // OR(  //Done
#define ORNE 0x17  // ORN( //Done
#define XORN 0x18  //Done
#define XORE 0x19  // XOR(  //Done
#define XORNE 0x1A // XORN( //Done
#define LDN 0x1B   //Done
#define STN 0x1C   //Done
#define ADDE 0x1D  //ADD(  //Done
#define SUBE 0x1E  //Done
#define MULE 0x1F  //Done
#define DIVE 0x20  //Done
#define GTE 0x21   //Done
#define GEE 0x22   //Done
#define EQE 0x23   //Done
#define NEE 0x24   //Done
#define LEE 0x25   //Done
#define LTE 0x26   //Done
#define JMP 0x27   //Done
#define JMPC 0x28  //Done
#define JMPCN 0x29 //Done
#define CAL 0x2A
#define CALC 0x2B
#define CALCN 0x2C
#define RET 0x2D //Done
#define RETC 0x2E
#define RETCN 0x2F
#define EXC 0x30 //Expression close -> ) -> recursion   //Done

/*

  Beispiel zum Testen:

  LD 13
  ADD 5
  MUL 10
  ST a

  a == (13 + 5) x 10;

  Umgewandelt:

  0113;085;0A10;02a;


*/

/**************OTHER DEFINITIONS DEFINITION**************/

#define TEMP_SIZE 100

using std::string;

using namespace std;

#define _CRT_SECURE_NO_WARNINGS

/****************IMPLEMENTATIONS***********************/

void EIL::createStack()
{
  il_stack = {{0}, 0, STACK_SIZE};
  il_stack.maxSize = STACK_SIZE;
  il_stack.size = 0;
}

void EIL::stackPush(char *data)
{
  strcpy(il_stack.mem[il_stack.size], data);
  il_stack.size++;
}

char *EIL::stackPop()
{
  il_stack.size--;
  return il_stack.mem[il_stack.size];
}

void EIL::freeStack()
{
  memset(il_stack.mem, 0, sizeof(il_stack.mem));
  il_stack.size = 0;
}

uint8_t EIL::isFull()
{
  return (il_stack.size == il_stack.maxSize);
}

uint8_t EIL::isEmpty()
{
  return (il_stack.size == 0);
}

void EIL::addVariable(const char *varname, const char *value)
{
  strcpy(il_variables.name[il_variables.size], varname);
  strcpy(il_variables.runtimeVar[il_variables.size], value);
  il_variables.isExternal[il_variables.size] = 0;
  il_variables.size = il_variables.size + 1;
}

char *EIL::getVariable(const char *varname)
{
  //Serial.println("Received in getVariable: " + String(varname));
  for (uint16_t i = 0; i <= il_variables.size; i++)
  {
    if (strcmp(il_variables.name[i], varname) == 0)
    {
      if (il_variables.isExternal[i] == 1)
      {
        return il_variables.externVar[i]((char *)"LD");
      }
      else if (il_variables.isExternal[i] == 0)
      {

        return il_variables.runtimeVar[i];
      }
    }
  }
  //Serial.println("Returning Same Varname");
  return (char *)varname;
}
void EIL::changeVariable(const char *varname, const char *newVal)
{
  for (uint16_t i = 0; i <= il_variables.size; i++)
  {
    if (strcmp(il_variables.name[i], varname) == 0)
    {
      if (il_variables.isExternal[i] == 1)
      {
        il_variables.externVar[i]((char *)newVal);
      }
      else if (il_variables.isExternal[i] == 0)
      {
        strcpy(il_variables.runtimeVar[i], newVal);
      }
    }
  }
}
uint8_t EIL::isVariableAvailable(const char *varname)
{
  for (uint16_t i = 0; i <= il_variables.size; i++)
  {
    //Serial.println(il_variables->name[i]);
    if (strcmp(il_variables.name[i], varname) == 0)
    {
      return 1;
    }
  }
  return 0;
}

void EIL::initFunctions()
{
  il_functions = (functions *)malloc(STACK_SIZE);
  il_functions->name = (const char **)malloc(STACK_SIZE);
  il_functions->size = 0;
}

void EIL::registerFunction(const char *name, uint8_t code, void (*func)(char *))
{
  //Serial.println("Got into registerFunction");
  il_functions->functions[il_functions->size] = (*func);
  il_functions->name[il_functions->size] = (const char *)malloc(STACK_SIZE);
  il_functions->name[il_functions->size] = name;
  il_functions->code[il_functions->size] = code;
  il_functions->size += 1;
}

void EIL::initVariables()
{
  //il_variables = (variables *)malloc(STACK_SIZE);
  il_variables = {0, 0, 0, 0, 0};
}

void EIL::registerVariable(const char *name, char *(*var)(char *))
{
  il_variables.externVar[il_variables.size] = (*var);
  strcpy(il_variables.name[il_variables.size], name);
  il_variables.isExternal[il_variables.size] = 1;
  il_variables.size += 1;
}

char *EIL::fetchVariable(const char *var, const char *op)
{
  for (uint8_t i = 0; i <= il_variables.size; i++)
  {
    if (strcmp(var, il_variables.name[i]) == 0)
    {
      return il_variables.externVar[i]((char *)op);
    }
  }
  return (char *)var;
}

void EIL::splitAndAssignList(char *instruction, uint8_t shouldExecute)
{
  if (lines == nullptr)
  {
    lines = (char **)malloc(8192);
  }
  n_lines = 0;
  int offset = 0;
  string testString = (std::string)instruction;
  size_t limiter = testString.find_first_of(";");
  while (limiter != std::string::npos)
  {
    string tempString = testString.substr(offset, limiter - offset);

    if (tempString.at(0) == '|')
      tempString.erase(0, 1);
    lines[n_lines] = (char *)(malloc(STACK_SIZE));
    //lines[array_counter] = (char*)tempString.c_str();
    strcpy(lines[n_lines], tempString.c_str());

    //if (shouldExecute)  executeCmd(lines[array_counter]);
    offset = limiter + 1;
    limiter = testString.find_first_of(";", offset);
    string op = tempString.substr(0, 2);

    n_lines++;
  }
  return;
}

//converting a string like A1 into a real HEX like 0xA1 represented by a uint8_t type

uint8_t EIL::convertOPChartoHex(char *op)
{
  uint8_t hex = 0;

  for (int i = 0; i < 2; i++)
  {
    switch (op[i])
    {
    case '0':
      hex |= (i == 0) ? 0x00 : 0x00;
      //cout << "0\n";
      break;
    case '1':
      hex |= (i == 0) ? 0x10 : 0x01;
      //cout << "1\n";
      break;
    case '2':
      hex |= (i == 0) ? 0x20 : 0x02;
      //cout << "2\n";
      break;
    case '3':
      hex |= (i == 0) ? 0x30 : 0x03;
      //cout << "3\n";
      break;
    case '4':
      hex |= (i == 0) ? 0x40 : 0x04;
      //cout << "4\n";
      break;
    case '5':
      hex |= (i == 0) ? 0x50 : 0x05;
      //cout << "5\n";
      break;
    case '6':
      hex |= (i == 0) ? 0x60 : 0x06;
      //cout << "6\n";
      break;
    case '7':
      hex |= (i == 0) ? 0x70 : 0x07;
      //cout << "7\n";
      break;
    case '8':
      hex |= (i == 0) ? 0x80 : 0x08;
      //cout << "8\n";
      break;
    case '9':
      hex |= (i == 0) ? 0x90 : 0x09;
      //cout << "9\n";
      break;
    case 'A':
      hex |= (i == 0) ? 0xA0 : 0x0A;
      //cout << "A\n";
      break;
    case 'B':
      hex |= (i == 0) ? 0xB0 : 0x0B;
      //cout << "B\n";
      break;
    case 'C':
      hex |= (i == 0) ? 0xC0 : 0x0C;
      //cout << "C\n";
      break;
    case 'D':
      hex |= (i == 0) ? 0xD0 : 0x0D;
      //cout << "D\n";
      break;
    case 'E':
      hex |= (i == 0) ? 0xE0 : 0x0E;
      //cout << "E\n";
      break;
    case 'F':
      hex |= (i == 0) ? 0xF0 : 0x0F;
      //cout << "F\n";
      break;
    }
  }
  //cout << (int)hex << "\n";
  return hex;
}

//This part check which datatype the argument has, which then gets stored next to the stack
//run through every single char and check if it is a Float (or Real as its called in IL)

uint8_t EIL::isArgFloat(char *arg, uint8_t op)
{
  //Serial.println("Checking float.");
  uint8_t res = 0;
  uint8_t separation_token = 0; //this needs to be set to one if a dot is present

  // check if the argument is purely made out of only digits
  // floats (or real) should be distinguished by the dot
  // also check with operator can do operations with float

  for (uint16_t i = 0; i < strlen(arg); i++)
  {
    if ((((uint16_t)arg[i] >= 48 && (uint16_t)arg[i] <= 57) || (int)arg[i] == 46) && (op == (uint8_t)LD || op == (uint8_t)ADD || op == (uint8_t)SUB || op == (uint8_t)MUL || op == (uint8_t)DIV || op == (uint8_t)GT || op == (uint8_t)GE || op == (uint8_t)EQ || op == (uint8_t)NE || op == (uint8_t)LE || op == (uint8_t)LT))
    {
      res = 1;
      if ((int)arg[i] == 46)
      {
        separation_token = 1;
      }
    }
    else
    {
      res = 0;
      break;
    }
  }
  return (separation_token == 1) ? res : 0;
}
uint8_t EIL::isArgInt(char *arg, uint8_t op)
{
  for (uint16_t i = 0; i < (uint16_t)strlen(arg); i++)
  {
    if (((int)arg[i] >= 48 && (int)arg[i] <= 57 || (int)arg[i] == 45) && (op == (uint8_t)LD || op == (uint8_t)ST || op == (uint8_t)ADD || op == (uint8_t)SUB || op == (uint8_t)MUL || op == (uint8_t)DIV || op == (uint8_t)GT || op == (uint8_t)GE || op == (uint8_t)EQ || op == (uint8_t)NE || op == (uint8_t)LE || op == (uint8_t)LT))
    {
      return 1;
    }
    else
    {
      return 0;
      break;
    }
  }
  return 0;
}

uint8_t EIL::isArgNum(char *arg, uint8_t op)
{
  for (uint16_t i = 0; (uint16_t)strlen(arg); i++)
  {
    if (((int)arg[i] >= 48 && (int)arg[i] <= 57) || (int)arg[i] == 46 || (int)arg[i] == 45)
    {
      return 1;
    }
    else
      return 0;
  }
  return 0;
}

uint8_t EIL::isArgBool(char *arg, uint8_t op)
{
  if ((strcmp(arg, "TRUE") == 0 || strcmp(arg, "FALSE") == 0) && (op == (uint8_t)LD || op == (uint8_t)AND || op == (uint8_t)OR || op == (uint8_t)XOR || op == (uint8_t)S || op == (uint8_t)R || op == (uint8_t)EQ || op == (uint8_t)NE))
  {
    return 1;
  }
  else
  {
    return 0;
  }
  return 0;
}

uint8_t EIL::isArgVar(char *arg, uint8_t op)
{
  for (uint16_t i = 0; i < strlen(arg); i++)
  {
    if (((int)arg[i] >= 32 && (int)arg[i] <= 122) //a-z
        && (op == (uint8_t)LD || op == (uint8_t)ST || op == (uint8_t)GT || op == (uint8_t)GE || op == (uint8_t)EQ || op == (uint8_t)NE || op == (uint8_t)LE || op == (uint8_t)LT))
    {
      return 1;
    }
    else
    {
      return 0;
      break;
    }
  }
  return 0;
}

void EIL::removeChar(char *s, char c)
{
  int j, n = strlen(s);
  for (int i = j = 0; i < n; i++)
    if (s[i] != c)
      s[j++] = s[i];

  s[j] = '\0';
}

void EIL::getParameters(char **str, char **params)
{
  removeChar(*str, '[');
  removeChar(*str, ']');
  char delimiter[] = ",";
  char *ptr;
  int idx = 0;

  ptr = strtok(*str, delimiter);

  while (ptr != NULL)
  {
    params[idx] = (char *)malloc(128);
    strcpy(params[idx], ptr);
    idx += 1;
    ptr = strtok(NULL, delimiter);
  }
}

void EIL::execute(char **lines)
{
  programCounter = 0;
  bool reExec = false;
  char *reExecLine = {0};
  char *line = (char *)malloc(1024);
  while (programCounter < n_lines)
  {
    strcpy(line, (reExec == true) ? reExecLine : lines[programCounter]);
    //Serial.println(lines[programCounter]);
    String op = String(line).substring(0, 2);

    uint8_t opHEX = convertOPChartoHex((char *)op.c_str());
    //Serial.println("Got here2");
    String _arg = String(line).substring(2, strlen(line));
    //Serial.println("Got here3");
    string arg = (std::string)getVariable(_arg.c_str());
    //Serial.println("Got here4");

    //Serial.printf("%s %s ", op.c_str(), _arg.c_str());
    //Serial.println();

    /*
    //Check what data type the argument has, if it has a certain type
    if (isArgFloat((char*)arg.c_str(), opHEX)) {
      cout << "Argument is a Number. "  << "\n";
    }
    else if (isArgInt((char*)arg.c_str(), opHEX)) {
      cout << "Argument is an Integer. " << "\n";
    }
    else if (isArgBool((char*)arg.c_str(), opHEX)) {
      cout << "Argument is a Boolean. " << "\n";
    }
    else {
      cout << "[EIL
    ] Argument is of an unknown type. " << "\n";
    }*/

    //This is where the magic happens
    if (opHEX == (uint8_t)LD)
    {
      //Serial.print("LD ");
      //Serial.println(arg.c_str());
      //cout << "LD " << (char*)arg.c_str() << "\n";
      if (isArgVar((char *)_arg.c_str(), opHEX) && !isArgBool((char *)_arg.c_str(), opHEX))
      {
        stackPush(getVariable((char *)arg.c_str()));
        //Serial.println("Pushed to stack: var or not bool");
      }
      else
      {
        stackPush((char *)arg.c_str());
        //Serial.println("Pushed to stack: else");
      }
    }
    else if (opHEX == (uint8_t)LDN)
    {
      if (isArgBool((char *)arg.c_str(), opHEX))
      {
        if (strcmp((char *)arg.c_str(), "TRUE") == 0)
        {
          stackPush((char *)"FALSE");
        }
        else if (strcmp((char *)arg.c_str(), "FALSE") == 0)
        {
          stackPush((char *)"TRUE");
        }
      }
    }

    else if (opHEX == (uint8_t)ST)
    {
      //Serial.print("ST ");
      //Serial.println(arg.c_str());
      //cout << "ST " << arg << "\n" << "\n";
      char *var = stackPop();
      //Serial.println(var);
      if (isArgVar((char *)_arg.c_str(), opHEX) && !isArgBool((char *)_arg.c_str(), opHEX))
      {
        //stackPush(var, (char *)arg.c_str());
        if (isVariableAvailable((char *)_arg.c_str()) == 1)
        {
          //Serial.println("Var is available.");
          changeVariable(_arg.c_str(), var);
        }
        else
        {
          addVariable(_arg.c_str(), var);
          //Serial.println("Var is not available. Added it.");
        }
      }
      else
      {
        stackPush(var);
      }
    }
    else if (opHEX == (uint8_t)STN)
    {
      char *var = stackPop();
      if (strcmp(var, "FALSE") == 0)
        strcpy(var, "TRUE");

      else if (strcmp(var, "TRUE") == 0)
        strcpy(var, "FALSE");

      if (isArgBool((char *)_arg.c_str(), opHEX))
      {
        if (isVariableAvailable((char *)_arg.c_str()) == 1)
        { // WTF
          changeVariable(_arg.c_str(), var);
        }
        else
        {
          addVariable(_arg.c_str(), var);
        }
      }
      else
      {
        stackPush(var);
      }
    }
    else if (opHEX == (uint8_t)S)
    {
      if (isVariableAvailable((char *)_arg.c_str()) == 1)
      {
        changeVariable(_arg.c_str(), (char *)"TRUE");
      }
      else
      {
        addVariable(_arg.c_str(), (char *)"TRUE");
      }
    }
    else if (opHEX == (uint8_t)R)
    {
      if (isVariableAvailable((char *)_arg.c_str()) == 1)
      {
        changeVariable(_arg.c_str(), (char *)"FALSE");
      }
      else
      {
        addVariable(_arg.c_str(), (char *)"FALSE");
      }
    }
    else if (opHEX == (uint8_t)AND)
    { //AND
      //cout << "AND " << arg << "\n";
      char *poped = stackPop();
      if (isArgBool(poped, opHEX) && isArgBool((char *)arg.c_str(), opHEX))
      {
        bool operand1 = (strcmp(poped, "TRUE") == 0) ? true : false;
        bool operand2 = (strcmp(arg.c_str(), "TRUE") == 0) ? true : false;

        if (operand1 && operand2)
          stackPush((char *)"TRUE");
        else
          stackPush((char *)"FALSE");
      }
      else
      {
        stackPush((char *)"NULL");
      }
    }
    else if (opHEX == (uint8_t)OR)
    { //OR
      //cout << "OR " << arg << "\n";
      char *poped = stackPop();
      if (isArgBool(poped, opHEX) && isArgBool((char *)arg.c_str(), opHEX))
      {
        bool operand1 = (strcmp(poped, "TRUE") == 0) ? true : false;
        bool operand2 = (strcmp(arg.c_str(), "TRUE") == 0) ? true : false;

        if (operand1 || operand2)
          stackPush((char *)"TRUE");
        else
          stackPush((char *)"FALSE");
      }
      else
      {
        stackPush((char *)"NULL");
      }
    }
    else if (opHEX == (uint8_t)XOR)
    { //XOR
      //cout << "XOR " << arg << "\n";
      char *poped = stackPop();
      if (isArgBool(poped, opHEX) && isArgBool((char *)arg.c_str(), opHEX))
      {
        bool operand1 = (strcmp(poped, "TRUE") == 0) ? true : false;
        bool operand2 = (strcmp(arg.c_str(), "TRUE") == 0) ? true : false;

        if ((operand1 && !operand2) || (!operand1 && operand2))
          stackPush((char *)"TRUE");
        else
          stackPush((char *)"FALSE");
      }
      else
      {
        stackPush((char *)"NULL");
      }
    }
    else if (opHEX == (uint8_t)ADD)
    { //ADD
      char *operand1 = stackPop();
      //cout << "ADD " << arg <<"\n";
      //Serial.print("ADD ");
      //Serial.println(arg.c_str());
      char res[TEMP_SIZE];
      if (isArgFloat(operand1, opHEX) || isArgFloat((char *)arg.c_str(), opHEX))
      {
        float resFloat = atof(operand1) + atof(arg.c_str());
        sprintf(res, "%0.2f", resFloat);
        stackPush(res);
      }
      else
      {
        int resInt = atoi(operand1) + atoi(arg.c_str());
        sprintf(res, "%d", resInt);
        stackPush(res);
      }
    }
    else if (opHEX == (uint8_t)SUB)
    { //SUB
      char *operand1 = stackPop();
      //cout << "ADD " << arg <<"\n";
      //Serial.print("SUB ");
      //Serial.println(arg.c_str());
      char res[TEMP_SIZE];
      if (isArgFloat(operand1, opHEX) || isArgFloat((char *)arg.c_str(), opHEX))
      {
        float resFloat = atof(operand1) - atof(arg.c_str());
        sprintf(res, "%0.2f", resFloat);
        stackPush(res);
      }
      else
      {
        int resInt = atoi(operand1) - atoi(arg.c_str());
        sprintf(res, "%d", resInt);
        stackPush(res);
      }
    }
    else if (opHEX == (uint8_t)MUL)
    { //MUL
      char *operand1 = stackPop();
      //cout << "ADD " << arg <<"\n";
      //Serial.print("MUL ");
      //Serial.println(arg.c_str());
      char res[TEMP_SIZE];

      if (isArgFloat(operand1, opHEX) || isArgFloat((char *)arg.c_str(), opHEX))
      {
        //char *res = (char *)malloc(TEMP_SIZE);
        float resFloat = atof(operand1) * atof(arg.c_str());
        sprintf(res, "%0.2f", resFloat);
        stackPush(res);
      }
      else
      {
        int resInt = atoi(operand1) * atoi(arg.c_str());
        sprintf(res, "%d", resInt);
        stackPush(res);
      }
    }
    else if (opHEX == (uint8_t)DIV)
    { //DIV
      char *operand1 = stackPop();
      //cout << "ADD " << arg <<"\n";
      //Serial.print("DIV ");
      //Serial.println(arg.c_str());
      char res[TEMP_SIZE];

      if (isArgFloat(operand1, opHEX) || isArgFloat((char *)arg.c_str(), opHEX))
      {
        float resFloat = atof(operand1) / atof(arg.c_str());
        sprintf(res, "%0.2f", resFloat);
        stackPush(res);
      }
      else
      {
        int resInt = atoi(operand1) / atoi(arg.c_str());
        sprintf(res, "%d", resInt);
        stackPush(res);
      }
    }
    else if (opHEX == (uint8_t)GT)
    { // >
      //cout << "GT " << arg << "\n";
      //Serial.print("GT ");
      //Serial.println(arg.c_str());
      char *operand1 = stackPop();
      if (isArgVar((char *)arg.c_str(), opHEX))
        arg = (std::string)getVariable((char *)arg.c_str());
      if (isArgNum(operand1, opHEX))
      {
        stackPush((atof(operand1) > atof((char *)arg.c_str())) ? (char *)"TRUE" : (char *)"FALSE");
      }
    }
    else if (opHEX == (uint8_t)GE)
    { // >=
      //cout << "GE " << arg << "\n";
      //Serial.print("GE ");
      //Serial.println(arg.c_str());
      char *operand1 = stackPop();
      if (isArgVar((char *)arg.c_str(), opHEX))
        arg = (std::string)getVariable((char *)arg.c_str());
      if (isArgNum(operand1, opHEX))
      {
        stackPush((atof(operand1) >= atof((char *)arg.c_str())) ? (char *)"TRUE" : (char *)"FALSE");
      }
    }
    else if (opHEX == (uint8_t)EQ)
    { // ==
      //cout << "EQ " << arg << "\n";
      //Serial.print("EQ ");
      //Serial.println(arg.c_str());
      char *operand1 = stackPop();
      if (isArgVar((char *)arg.c_str(), opHEX))
        arg = (std::string)getVariable((char *)arg.c_str());
      if (isArgNum(operand1, opHEX))
      {
        stackPush((atof(operand1) == atof((char *)arg.c_str())) ? (char *)"TRUE" : (char *)"FALSE");
      }
    }
    else if (opHEX == (uint8_t)NE)
    { // !=
      //cout << "NE " << arg << "\n";
      //Serial.print("NE ");
      //Serial.println(arg.c_str());
      char *operand1 = stackPop();
      if (isArgVar((char *)arg.c_str(), opHEX))
        arg = (std::string)getVariable((char *)arg.c_str());
      if (isArgNum(operand1, opHEX))
      {
        stackPush((atof(operand1) != atof((char *)arg.c_str())) ? (char *)"TRUE" : (char *)"FALSE");
      }
    }
    else if (opHEX == (uint8_t)LE)
    { // <=
      //cout << "LE " << arg << "\n";
      //Serial.print("LE ");
      //Serial.println(arg.c_str());
      char *operand1 = stackPop();
      if (isArgVar((char *)arg.c_str(), opHEX))
        arg = (std::string)getVariable((char *)arg.c_str());
      if (isArgNum(operand1, opHEX))
      {
        stackPush((atof(operand1) <= atof((char *)arg.c_str())) ? (char *)"TRUE" : (char *)"FALSE");
      }
    }
    else if (opHEX == (uint8_t)LT)
    { // <
      //cout << "LT " << arg << "\n";
      //Serial.print("LT ");
      //Serial.println(arg.c_str());
      char *operand1 = stackPop();
      if (isArgVar((char *)arg.c_str(), opHEX))
        arg = (std::string)getVariable((char *)arg.c_str());
      if (isArgNum(operand1, opHEX))
      {
        stackPush((atof(operand1) < atof((char *)arg.c_str())) ? (char *)"TRUE" : (char *)"FALSE");
      }
    }
    else if (opHEX == (uint8_t)ANDN)
    {
      char *poped = stackPop();
      if (isArgBool(poped, opHEX) && isArgBool((char *)arg.c_str(), opHEX))
      {
        bool operand1 = (strcmp(poped, "TRUE") == 0) ? true : false;
        bool operand2 = (strcmp(arg.c_str(), "TRUE") == 0) ? true : false;

        if (operand1 && operand2)
          stackPush((char *)"FALSE");
        else
          stackPush((char *)"TRUE");
      }
      else
      {
        stackPush((char *)"NULL");
      }
    }
    else if (opHEX == (uint8_t)ANDE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)AND);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)ANDNE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)ANDN);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)ORN)
    {
      char *poped = stackPop();
      if (isArgBool(poped, opHEX) && isArgBool((char *)arg.c_str(), opHEX))
      {
        bool operand1 = (strcmp(poped, "TRUE") == 0) ? true : false;
        bool operand2 = (strcmp(arg.c_str(), "TRUE") == 0) ? true : false;

        if (operand1 || operand2)
          stackPush((char *)"FALSE");
        else
          stackPush((char *)"TRUE");
      }
      else
      {
        stackPush((char *)"NULL");
      }
    }
    else if (opHEX == (uint8_t)ORE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)OR);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)ORNE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)ORN);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)XORN)
    {
      char *poped = stackPop();
      if (isArgBool(poped, opHEX) && isArgBool((char *)arg.c_str(), opHEX))
      {
        bool operand1 = (strcmp(poped, "TRUE") == 0) ? true : false;
        bool operand2 = (strcmp(arg.c_str(), "TRUE") == 0) ? true : false;

        if ((operand1 && !operand2) || (!operand1 && operand2))
          stackPush((char *)"FALSE");
        else
          stackPush((char *)"TRUE");
      }
      else
      {
        stackPush((char *)"NULL");
      }
    }
    else if (opHEX == (uint8_t)XORE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)XOR);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)XORNE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)XORN);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)ADDE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)ADD);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)SUBE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)SUB);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)MULE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)MUL);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)DIVE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)DIV);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)GTE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)GT);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)GEE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)GE);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)EQE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)EQ);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)NEE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)NE);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)LEE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)LE);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)LTE)
    {
      char opOnClose[2];
      sprintf(opOnClose, "%d", (uint8_t)LT);
      stackPush((char *)opOnClose);
      stackPush((char *)arg.c_str());
    }
    else if (opHEX == (uint8_t)EXC)
    {
      char *exResult = stackPop();
      char *exOperand = stackPop();
      strcat(exResult, exOperand);
      strcpy(reExecLine, exResult);
      reExec = true;
    }
    else if (opHEX == (uint8_t)JMP)
    {
      //uint16_t pos = getMarkerPos((char *)arg.c_str());
      Serial.println("Jumping");
      programCounter = atoi(arg.c_str());
      continue;
    }
    else if (opHEX == (uint8_t)JMPC)
    {
      if (strcmp(stackPop(), "TRUE") == 0)
      {
        programCounter = atoi(arg.c_str());
        continue;
      }
    }
    else if (opHEX == (uint8_t)JMPCN)
    {
      if (strcmp(stackPop(), "FALSE") == 0)
      {
        programCounter = atoi(arg.c_str());
        continue;
      }
    }
    else
    {
      for (uint8_t i = 0; i <= il_functions->size; i++)
      {
        if (opHEX == il_functions->code[i])
        {
          //cout << il_functions->name[i] << " " << (char*)arg.c_str() << "\n";
          //Serial.print(String(opHEX));
          //Serial.println(arg.c_str());
          il_functions->functions[i]((char *)arg.c_str());
        }
      }
    }
    programCounter = (reExec == true) ? programCounter : (programCounter + 1);
    reExec = false;
  }
  free(line);
}

void EIL::initVM()
{
  //std::cout << "Initializing VM\n";
  createStack();
  initFunctions();
  initVariables();
  //Serial.println("Got into initVM");
}

void EIL::insertScript(const char *script_)
{
  script = (char *)script_;
  splitAndAssignList(script, true);
  //splitAndAssignList(script, true);
  //Serial.println("Got into insertScript");
  //Serial.println("splitAndAssignList succesful.");
  //Serial.println("Poping variable \"a\" : " + (String)stackPop("a") + "\n");    // Operations
  //Serial.println("Poping variable \"b\" : " + (String)stackPop("b") + "\n");    // Variables saving
  //Serial.println("Poping variable \"d\" : " + (String)stackPop("d") + "\n");    // Some more Conditions
  //execute(lines);
  //Serial.println("Poping variable \"a\" : " + (String)stackPop("a") + "\n");
}

void EIL::handleVM()
{
  execute(lines);
  //Serial.println("Got into handleVM");
  /*
  Serial.println("execute succesful.");
  Serial.println("Getting variable \"a\" : " + (String)getVariable("a") + "\n"); // Operations
  Serial.println("Getting a non-existing variable:" + (String)getVariable("x") + "\n");
  Serial.println("Getting variable \"b\" : " + (String)getVariable("b") + "\n"); // Operations
  Serial.println("Getting variable \"c\" : " + (String)getVariable("c") + "\n"); // Variables saving
  Serial.println("Getting variable \"d\" : " + (String)getVariable("d") + "\n"); // Some more Conditions
  Serial.println("Getting variable \"e\" : " + (String)getVariable("e") + "\n"); // Some more Conditions
  Serial.println("Getting variable \"f\" : " + (String)getVariable("f") + "\n"); // Some more Conditions
  Serial.println("Getting variable \"g\" : " + (String)getVariable("g") + "\n"); // Some more Conditions
  */
  //executeCmd(loop);
  //cout << "[EIL] Executing Loop";
  //Serial.println("Getting variable \"D\" : " + (String)getVariable("c") + "\n"); // Some more Conditions
}
