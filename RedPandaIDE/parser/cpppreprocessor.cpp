#include "cpppreprocessor.h"
#include "../utils.h"

CppPreprocessor::CppPreprocessor(QObject *parent) : QObject(parent)
{
    mOperators.append("*");
    mOperators.append("/");
    mOperators.append("+");
    mOperators.append("-");
    mOperators.append("<");
    mOperators.append("<=");
    mOperators.append(">");
    mOperators.append(">=");
    mOperators.append("==");
    mOperators.append("!=");
    mOperators.append("&");
    mOperators.append("^");
    mOperators.append("|");
    mOperators.append("&&");
    mOperators.append("||");
    mOperators.append("and");
    mOperators.append("or");
}

QString CppPreprocessor::getNextPreprocessor()
{

    skipToPreprocessor(); // skip until # at start of line
    int preProcFrom = mIndex;
    if (preProcFrom >= mBuffer.count())
        return ""; // we've gone past the final #preprocessor line. Yay
    skipToEndOfPreprocessor();
    int preProcTo = mIndex;

    // Calculate index to insert defines in in result file
    mPreProcIndex = (mResult.count() - 1) + 1; // offset by one for #include rootfile

    // Assemble whole line, including newlines
    QString result;
    for (int i=preProcFrom;i<=preProcTo;i++) {
        result+=mBuffer[i]+'\n';
        mResult.append("");// defines resolve into empty files, except #define and #include
    }
    // Step over
    mIndex++;
    return result;
}

void CppPreprocessor::handleBranch(const QString &line)
{
    if (line.startsWith("ifdef")) {
//        // if a branch that is not at our level is false, current branch is false too;
//        for (int i=0;i<=mBranchResults.count()-2;i++) {
//            if (!mBranchResults[i]) {
//                setCurrentBranch(false);
//                return;
//            }
//        }
        if (!getCurrentBranch()) {
            setCurrentBranch(false);
        } else {
            constexpr int IFDEF_LEN = 5; //length of ifdef;
            QString name = line.mid(IFDEF_LEN).trimmed();
            int dummy;
            setCurrentBranch( getDefine(name,dummy)!=nullptr );

        }
    } else if (line.startsWith("ifndef")) {
//        // if a branch that is not at our level is false, current branch is false too;
//        for (int i=0;i<=mBranchResults.count()-2;i++) {
//            if (!mBranchResults[i]) {
//                setCurrentBranch(false);
//                return;
//            }
//        }
        if (!getCurrentBranch()) {
            setCurrentBranch(false);
        } else {
            constexpr int IFNDEF_LEN = 6; //length of ifndef;
            QString name = line.mid(IFNDEF_LEN).trimmed();
            int dummy;
            setCurrentBranch( getDefine(name,dummy)==nullptr );
        }
    } else if (line.startsWith("if")) {
        //        // if a branch that is not at our level is false, current branch is false too;
        //        for (int i=0;i<=mBranchResults.count()-2;i++) {
        //            if (!mBranchResults[i]) {
        //                setCurrentBranch(false);
        //                return;
        //            }
        //        }
        if (!getCurrentBranch()) {// we are already inside an if that is NOT being taken
            setCurrentBranch(false);// so don't take this one either
        } else {
            constexpr int IF_LEN = 2; //length of if;
            QString ifLine = line.mid(IF_LEN).trimmed();

            bool testResult = evaludateIf(ifLine);
            setCurrentBranch(testResult);
        }
    } else if (line.startsWith("else")) {
        bool oldResult = getCurrentBranch(); // take either if or else
        removeCurrentBranch();
        setCurrentBranch(!oldResult);
    } else if (line.startsWith("elif")) {
        bool oldResult = getCurrentBranch(); // take either if or else
        removeCurrentBranch();
        if (oldResult) { // don't take this one, if  previous has been taken
            setCurrentBranch(false);
        } else {
            constexpr int ELIF_LEN = 4; //length of if;
            QString ifLine = line.mid(ELIF_LEN).trimmed();
            bool testResult = evaludateIf(ifLine);
            setCurrentBranch(testResult);
        }
    } else if (line.startsWith("endif")) {
        removeCurrentBranch();
    }
}

QString CppPreprocessor::expandMacros(const QString &line, int depth)
{
    //prevent infinit recursion
    if (depth > 20)
        return line;
    QString word;
    QString newLine;
    int lenLine = line.length();
    int i=0;
    while (i< lenLine) {
        QChar ch=line[i];
        if (isWordChar(ch)) {
            word += ch;
        } else {
            if (!word.isEmpty()) {
                expandMacro(line,newLine,word,i,depth);
            }
            word = "";
            if (i< lenLine) {
                newLine += ch;
            }
        }
        i++;
    }
    if (!word.isEmpty()) {
        expandMacro(line,newLine,word,i,depth);
    }
    return newLine;
}

void CppPreprocessor::expandMacro(const QString &line, QString &newLine, QString &word, int &i, int depth)
{
    int lenLine = line.length();
    if (word == "__attribute__") {
        //skip gcc __attribute__
        while ((i<lenLine) && (line[i] == ' ' || line[i]=='\t'))
            i++;
        if ((i<lenLine) && (line[i]=="(")) {
            int level=0;
            while (i<lenLine) {
                switch(line[i].unicode()) {
                    case '(':
                        level++;
                    break;
                    case ')':
                        level--;
                    break;
                }
                i++;
                if (level==0)
                    break;
            }
        }
    } else {
        int index;
        PDefine define = getDefine(word,index);
        if (define && define->args=="" && (!define->isMultiLine)) {
            //newLine:=newLine+RemoveGCCAttributes(define^.Value);
            if (define->value != word )
              newLine += expandMacros(define->value,depth+1);
            else
              newLine += word;

        } else if (define && (!define->isMultiLine) && (define->args!="")) {
            while ((i<lenLine) && (line[i] == ' ' || line[i]=='\t'))
                i++;
            int argStart=-1;
            int argEnd=-1;
            if ((i<lenLine) && (line[i]=='(')) {
                argStart =i+1;
                int level=0;
                while (i<lenLine) {
                    switch(line[i].unicode()) {
                        case '(':
                            level++;
                        break;
                        case ')':
                            level--;
                        break;
                    }
                    i++;
                    if (level==0)
                        break;
                }
                if (level==0) {
                    argEnd = i-2;
                    QString args = line.mid(argStart,argEnd-argStart+1).trimmed();
                    QString formattedValue = expandFunction(define,args);
                    newLine += expandMacros(formattedValue,depth+1);
                }
            }
        } else {
            newLine += word;
        }
    }
}

PParsedFile CppPreprocessor::getInclude(int index)
{
    return mIncludes[index];
}

void CppPreprocessor::closeInclude()
{
    if (mIncludes.isEmpty())
        return;
    mIncludes.pop_back();

    if (mIncludes.isEmpty())
        return;
    PParsedFile parsedFile = mIncludes.back();

    // Continue where we left off
    mIndex = parsedFile->index;
    mFileName = parsedFile->fileName;
    // Point to previous buffer and start past the include we walked into
    mBuffer = parsedFile->buffer;
    while (mBranchResults.count()>parsedFile->branches) {
        mBranchResults.pop_back();
    }


    // Start augmenting previous include list again
    //fCurrentIncludes := GetFileIncludesEntry(fFileName);
    mCurrentIncludes = parsedFile->fileIncludes;

    // Update result file (we've left the previous file)
    mResult.append(
                QString("#include %1:%2").arg(parsedFile->fileName)
                .arg(parsedFile->index+1));
}

bool CppPreprocessor::getCurrentBranch()
{
    if (!mBranchResults.isEmpty())
        return mBranchResults.last();
    else
        return true;
}

QString CppPreprocessor::getResult()
{
    QString s;
    for (QString line:mResult) {
        s.append(line);
        s.append(lineBreak());
    }
    return s;
}

PFileIncludes CppPreprocessor::getFileIncludesEntry(const QString &fileName)
{
    return mIncludesList.value(fileName,PFileIncludes());
}

void CppPreprocessor::addDefinesInFile(const QString &fileName)
{
    if (mProcessed.contains(fileName))
        return;
    mProcessed.insert(fileName);

    //todo: why test this?
    if (!mScannedFiles.contains(fileName))
        return;

    PDefineMap defineList = mFileDefines.value(fileName, PDefineMap());

    if (defineList) {
        for (PDefine define: defineList->values()) {
            mDefines.insert(define->name,define);
        }
    }
    PFileIncludes fileIncludes = getFileIncludesEntry(fileName);
    if (fileIncludes) {
        for (QString s:fileIncludes->includeFiles.keys()) {
            addDefinesInFile(s);
        }
    }
}

bool CppPreprocessor::isWordChar(const QChar &ch)
{
    if (ch=='_' || (ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || (ch>='0' && ch<='9')) {
        return true;
    }
    return false;
}

bool CppPreprocessor::isIdentChar(const QChar &ch)
{
    if (ch=='_' || ch == '*' || ch == '&' || ch == '~' ||
            (ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || (ch>='0' && ch<='9')) {
        return true;
    }
    return false;
}

bool CppPreprocessor::isLineChar(const QChar &ch)
{
    return ch=='\r' || ch == '\n';
}

bool CppPreprocessor::isSpaceChar(const QChar &ch)
{
    return ch == ' ' || ch == '\t';
}

bool CppPreprocessor::isOperatorChar(const QChar &ch)
{

    switch(ch.unicode()) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '!':
    case '=':
    case '<':
    case '>':
    case '&':
    case '|':
    case '^':
        return true;
    default:
        return false;
    }
}

bool CppPreprocessor::isMacroIdentChar(const QChar &ch)
{
    return (ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || ch == '_';
}

bool CppPreprocessor::isDigit(const QChar &ch)
{
    return (ch>='0' || ch<='9');
}

QString CppPreprocessor::lineBreak()
{
    return "\n";
}

bool CppPreprocessor::evaluateIf(const QString &line)
{
    QString newLine = expandDefines(line); // replace FOO by numerical value of FOO
    return  removeSuffixes(evaluateExpression(newLine)), -1) > 0;
}

QString CppPreprocessor::expandDefines(QString line)
{
    int searchPos = 0;
    while (searchPos < line.length()) {
        // We have found an identifier. It is not a number suffix. Try to expand it
        if (isMacroIdentChar(line[searchPos]) && (
                    (searchPos == 1) || !isDigit(line[searchPos - 1]))) {
            int head = searchPos;
            int tail = searchPos;

            // Get identifier name (numbers are allowed, but not at the start
            while ((tail < line.length()) && (isMacroIdentChar(line[tail]) || isDigit(line[head])))
                tail++;
            QString name = line.mid(head,tail-head);
            int nameStart = head;
            int nameEnd = tail;

            if (name == "defined") {
                //expand define
                //tail = searchPos + name.length();
                while ((tail < line.length()) && isSpaceChar(line[tail]))
                    tail++; // skip spaces
                int defineStart;
                int defineEnd;

                // Skip over its arguments
                if ((tail < line.length()) && (line[tail]=='(')) {
                    //braced argument (next word)
                    defineStart = tail+1;
                    if (!skipBraces(line, tail)) {
                        line = ""; // broken line
                        break;
                    }
                } else {
                    //none braced argument (next word)
                    defineStart = tail;
                    if ((tail>=line.length()) || !isMacroIdentChar(line[searchPos])) {
                        line = ""; // broken line
                        break;
                    }
                    while ((tail < line.length()) && (isMacroIdentChar(line[tail]) || isDigit(line[tail])))
                        tail++;
                }
                name = line.mid(defineStart, defineEnd - defineStart);
                int dummy;
                PDefine define = getDefine(name,dummy);
                QString insertValue;
                if (!define) {
                    insertValue = "0";
                } else {
                    insertValue = "1";
                }
                // Insert found value at place
                line.remove(searchPos, tail-searchPos+1);
                line.insert(searchPos,insertValue);
            } else if ((name == "and") || (name == "or")) {
                searchPos = tail; // Skip logical operators
            }  else {
                 // We have found a regular define. Replace it by its value
                // Does it exist in the database?
                int dummy;
                PDefine define = getDefine(name,dummy);
                QString insertValue;
                if (!define) {
                    insertValue = "0";
                } else {
                    while ((tail < line.length()) && isSpaceChar(line[tail]))
                        tail++;// skip spaces
                    // It is a function. Expand arguments
                    if ((tail < line.length()) && (line[tail] == '(')) {
                        head=tail;
                        if (skipBraces(line, tail)) {
                            QString args = line.mid(head,tail-head+1);
                            insertValue = expandFunction(define,args);
                            nameEnd = tail+1;
                        } else {
                            line = "";// broken line
                            break;
                        }
                        // Replace regular define
                    } else {
                        if (!define->value.isEmpty())
                            insertValue = define->value;
                        else
                            insertValue = "0";
                    }
                }
                // Insert found value at place
                line.remove(nameStart, nameEnd - nameStart);
                line.insert(searchPos,insertValue);
            }
        } else {
            searchPos ++ ;
        }
    }
    return line;
}

bool CppPreprocessor::skipBraces(const QString &line, int &index, int step)
{
    int level = 0;
    while ((index >= 0) && (index < line.length())) { // Find the corresponding opening brace
        if (line[index] == '(') {
            level++;
        } else if (line[index] == ')') {
            level--;
        }
        if (level == 0)
            return true;
        index+=step;
    }
    return false;
}

QString CppPreprocessor::expandFunction(PDefine define, QString args)
{
    // Replace function by this string
    QString result = define->formatValue;
    if (args.startsWith('(')) {
        args.remove(0,1);
    }
    if (args.endsWith(')')) {
        args.remove(args.length()-1,1);
    }

    QStringList argValues = args.split(",");
    for (QString argValue:argValues) {
        result=result.arg(argValue);
    }
    return result;
}

bool CppPreprocessor::skipSpaces(const QString &expr, int &pos)
{
    while (pos<expr.length() && isSpaceChar(expr[pos]))
        pos++;
    return pos<expr.length();
}

/*
 * bit_xor_expr = bit_and_expr
     | bit_xor_expr "^" bit_and_expr
 */
bool CppPreprocessor::evalBitXorExpr(const QString &expr, int &result, int &pos)
{
    if (!evalBitAndExpr(expr,result,pos))
        return false;
    while (true) {
        if (!skipBraces(expr,result,pos))
            break;
        if (expr[pos]=='^') {
            pos++;
            int rightResult;
            if (!evalBitAndExpr(expr,result,pos))
                return false;
            result = result ^ rightResult;
        } else {
            break;
        }
    }
    return true;
}

/*
 * bit_or_expr = bit_xor_expr
     | bit_or_expr "|" bit_xor_expr
 */
bool CppPreprocessor::evalBitOrExpr(const QString &expr, int &result, int &pos)
{
    if (!evalBitXorExpr(expr,result,pos))
        return false;
    while (true) {
        if (!skipBraces(expr,result,pos))
            break;
        if (expr[pos] == '|') {
            pos++;
            int rightResult;
            if (!evalBitXorExpr(expr,result,pos))
                return false;
            result = result | rightResult;
        } else {
            break;
        }
    }
    return true;
}

/*
 * logic_and_expr = bit_or_expr
    | logic_and_expr "&&" bit_or_expr
 */
bool CppPreprocessor::evalLogicAndExpr(const QString &expr, int &result, int &pos)
{
    if (!evalBitOrExpr(expr,result,pos))
        return false;
    while (true) {
        if (!skipBraces(expr,result,pos))
            break;
        if (pos+1<expr.length() && expr[pos]=='&' && expr[pos+1] =='&') {
            pos+=2;
            int rightResult;
            if (!evalBitOrExpr(expr,rightResult,pos))
                return false;
            result = result && rightResult;
        } else {
            break;
        }
    }
    return true;
}

/*
 * logic_or_expr = logic_and_expr
    | logic_or_expr "||" logic_and_expr
 */
bool CppPreprocessor::evalLogicOrExpr(const QString &expr, int &result, int &pos)
{
    if (!evalLogicAndExpr(expr,result,pos))
        return false;
    while (true) {
        if (!skipBraces(expr,result,pos))
            break;
        if (pos+1<expr.length() && expr[pos]=='|' && expr[pos+1] =='|') {
            pos+=2;
            int rightResult;
            if (!evalLogicAndExpr(expr,rightResult,pos))
                return false;
            result = result || rightResult;
        } else {
            break;
        }
    }
    return true;
}

bool CppPreprocessor::evalExpr(const QString &expr, int &result, int &pos)
{
    return evalLogicOrExpr(expr,result,pos);
}

/* BNF for C constant expression evaluation
 * term = number
     | '(' expression ')'
unary_expr = term
     | '+' term
     | '-' term
     | '!' term
     | '~' term
mul_expr = term
     | mul_expr '*' term
     | mul_expr '/' term
     | mul_expr '%' term
add_expr = mul_expr
     | add_expr '+' mul_expr
     | add_expr '-' mul_expr
shift_expr = add_expr
     | shift_expr "<<" add_expr
     | shift_expr ">>" add_expr
relation_expr = shift_expr
     | relation_expr ">=" shift_expr
     | relation_expr ">" shift_expr
     | relation_expr "<=" shift_expr
     | relation_expr "<" shift_expr
equal_expr = relation_expr
     | equal_expr "==" relation_expr
     | equal_expr "!=" relation_expr
bit_and_expr = equal_expr
     | bit_and_expr "&" equal_expr
bit_xor_expr = bit_and_expr
     | bit_xor_expr "^" bit_and_expr
bit_or_expr = bit_xor_expr
     | bit_or_expr "|" bit_xor_expr
logic_and_expr = bit_or_expr
    | logic_and_expr "&&" bit_or_expr
logic_or_expr = logic_and_expr
    | logic_or_expr "||" logic_and_expr
    */

QString CppPreprocessor::evaluateExpression(QString line)
{
    //todo: improve this
    // Find the first closing brace (this should leave us at the innermost brace pair)
    while (true) {
        int head = line.indexOf(')');
        if (head<0)
            break;
        int tail = head;
        if (skipBraces(line, tail, -1)) { // find the corresponding opening brace
            QString resultLine = evaluateExpression(line.mid(tail + 1, head - tail - 1)); // evaluate this (without braces)
            line.remove(tail, head - tail + 1); // Remove the old part AND braces
            line.insert(tail,resultLine); // and replace by result
        } else {
            return "";
        }
    }

    // Then evaluate braceless part
    while (true) {
        int operatorPos;
        QString operatorToken = getNextOperator(operatorPos);
        if (operatorPos < 0)
            break;

        // Get left operand
        int tail = operatorPos - 1;
        while ((tail >= 0) && isSpaceChar(line[tail]))
            tail--; // skip spaces
        Head := Tail;
        while (Head >= 0) and (Line[Head] in IdentChars) do
          Dec(Head); // step over identifier
        LeftOp := Copy(Line, Head + 1, Tail - Head);
        EquatStart := Head + 1; // marks begin of equation

        // Get right operand
        Tail := OperatorPos + Length(OperatorToken) + 1;
        while (Tail <= Length(Line)) and (Line[Tail] in SpaceChars) do
          Inc(Tail); // skip spaces
        Head := Tail;
        while (Head <= Length(Line)) and (Line[Head] in IdentChars) do
          Inc(Head); // step over identifier
        RightOp := Copy(Line, Tail, Head - Tail);
        EquatEnd := Head; // marks begin of equation

        // Evaluate after removing length suffixes...
        LeftOpValue := StrToIntDef(RemoveSuffixes(LeftOp), 0);
        RightOpValue := StrToIntDef(RemoveSuffixes(RightOp), 0);
        if OperatorToken = '*' then
          ResultValue := LeftOpValue * RightOpValue
        else if OperatorToken = '/' then begin
          if RightOpValue = 0 then
            ResultValue := LeftOpValue
          else
            ResultValue := LeftOpValue div RightOpValue; // int division
        end else if OperatorToken = '+' then
          ResultValue := LeftOpValue + RightOpValue
        else if OperatorToken = '-' then
          ResultValue := LeftOpValue - RightOpValue
        else if OperatorToken = '<' then
          ResultValue := integer(LeftOpValue < RightOpValue)
        else if OperatorToken = '<=' then
          ResultValue := integer(LeftOpValue <= RightOpValue)
        else if OperatorToken = '>' then
          ResultValue := integer(LeftOpValue > RightOpValue)
        else if OperatorToken = '>=' then
          ResultValue := integer(LeftOpValue >= RightOpValue)
        else if OperatorToken = '==' then
          ResultValue := integer(LeftOpValue = RightOpValue)
        else if OperatorToken = '!=' then
          ResultValue := integer(LeftOpValue <> RightOpValue)
        else if OperatorToken = '&' then // bitwise and
          ResultValue := integer(LeftOpValue and RightOpValue)
        else if OperatorToken = '|' then // bitwise or
          ResultValue := integer(LeftOpValue or RightOpValue)
        else if OperatorToken = '^' then // bitwise xor
          ResultValue := integer(LeftOpValue xor RightOpValue)
        else if (OperatorToken = '&&') or (OperatorToken = 'and') then
          ResultValue := integer(LeftOpValue and RightOpValue)
        else if (OperatorToken = '||') or (OperatorToken = 'or') then
          ResultValue := integer(LeftOpValue or RightOpValue)
        else
          ResultValue := 0;

        // And replace by result in string form
        Delete(Line, EquatStart, EquatEnd - EquatStart);
        Insert(IntToStr(ResultValue), Line, EquatStart);
      end else
        break;
    end;
    Result := Line;
}

