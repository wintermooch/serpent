#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "parser.h"
#include "tokenize.h"

// Extended BEDMAS precedence order
int precedence(Node tok) {
    std::string v = tok.val;
    if (v == "!") return 0;
    else if (v=="^") return 1;
    else if (v=="*" || v=="/" || v=="@/" || v=="%" | v=="@%") return 2;
    else if (v=="+" || v=="-") return 3;
    else if (v=="<" || v==">" || v=="<=" || v==">=") return 4;
    else if (v=="@<" || v=="@>" || v=="@<=" || v=="@>=") return 4;
    else if (v=="&" || v=="|" || v=="xor" || v=="==") return 5;
    else if (v=="&&" || v=="and") return 6;    
    else if (v=="||" || v=="or") return 7;
    else if (v=="=") return 10;
    else return -1;
}

// Token classification for shunting-yard purposes
int toktype(Node tok) {
    if (tok.type == ASTNODE) return COMPOUND;
    std::string v = tok.val;
    if (v == "(" || v == "[") return LPAREN;
    else if (v == ")" || v == "]") return RPAREN;
    else if (v == ",") return COMMA;
    else if (v == ":") return COLON;
    else if (v == "!") return UNARY_OP;
    else if (precedence(tok) >= 0) return BINARY_OP;
    else return ALPHANUM;
}


// Converts to reverse polish notation
std::vector<Node> shuntingYard(std::vector<Node> tokens) {
    std::vector<Node> iq;
    for (int i = tokens.size() - 1; i >= 0; i--) {
        iq.push_back(tokens[i]);
    }
    std::vector<Node> oq;
    std::vector<Node> stack;
    Node prev, tok;
    int prevtyp, toktyp;
    
    while (iq.size()) {
        prev = tok;
        prevtyp = toktyp;
        tok = iq.back();
        toktyp = toktype(tok);
        iq.pop_back();
        // Alphanumerics go straight to output queue
        if (toktyp == ALPHANUM) {
            oq.push_back(tok);
        }
        // Left parens go on stack and output queue
        else if (toktyp == LPAREN) {
            if (prevtyp != ALPHANUM && prevtyp != RPAREN) {
                oq.push_back(token("id", tok.metadata));
            }
            Node fun = oq.back();
            oq.pop_back();
            oq.push_back(tok);
            oq.push_back(fun);
            stack.push_back(tok);
        }
        // If rparen, keep moving from stack to output queue until lparen
        else if (toktyp == RPAREN) {
            while (stack.size() && toktype(stack.back()) != LPAREN) {
                oq.push_back(stack.back());
                stack.pop_back();
            }
            if (stack.size()) stack.pop_back();
            oq.push_back(tok);
        }
        // If binary op, keep popping from stack while higher bedmas precedence
        else if (toktyp == UNARY_OP || toktyp == BINARY_OP) {
            if (tok.val == "-" && prevtyp != ALPHANUM && prevtyp != RPAREN) {
                oq.push_back(token("0", tok.metadata));
            }
            int prec = precedence(tok);
            while (stack.size() 
                  && toktype(stack.back()) == BINARY_OP 
                  && precedence(stack.back()) <= prec) {
                oq.push_back(stack.back());
                stack.pop_back();
            }
            stack.push_back(tok);
        }
        // Comma and colon mean finish evaluating the argument
        else if (toktyp == COMMA || toktyp == COLON) {
            while (stack.size() && toktype(stack.back()) != LPAREN) {
                oq.push_back(stack.back());
                stack.pop_back();
            }
            if (toktyp == COLON) oq.push_back(tok);
        }
    }
    while (stack.size()) {
        oq.push_back(stack.back());
        stack.pop_back();
    }
    return oq;
}

// Converts reverse polish notation into tree
Node treefy(std::vector<Node> stream) {
    std::vector<Node> iq;
    for (int i = stream.size() -1; i >= 0; i--) {
        iq.push_back(stream[i]);
    }
    std::vector<Node> oq;
    while (iq.size()) {
        Node tok = iq.back();
        iq.pop_back();
        int typ = toktype(tok);
        // If unary, take node off end of oq and wrap it with the operator
        // If binary, do the same with two nodes
        if (typ == UNARY_OP || typ == BINARY_OP) {
            std::vector<Node> args;
            int rounds = (typ == BINARY_OP) ? 2 : 1;
            for (int i = 0; i < rounds; i++) {
                args.push_back(oq.back());
                oq.pop_back();
            }
            std::vector<Node> args2;
            while (args.size()) {
                args2.push_back(args.back());
                args.pop_back();
            }
            oq.push_back(astnode(tok.val, args2, tok.metadata));
        }
        // If rparen, keep grabbing until we get to an lparen
        else if (toktype(tok) == RPAREN) {
            std::vector<Node> args;
            while (toktype(oq.back()) != LPAREN) {
                args.push_back(oq.back());
                oq.pop_back();
            }
            oq.pop_back();
            // We represent a[b] as (access a b)
            if (tok.val == "]") args.push_back(token("access", tok.metadata));
            std::string fun = args.back().val;
            args.pop_back();
            // We represent [1,2,3] as (array_lit 1 2 3)
            if (fun == "access" && args.size() && args.back().val == "id") {
                fun = "array_lit";
                args.pop_back();
            }
            std::vector<Node> args2;
            while (args.size()) {
                args2.push_back(args.back());
                args.pop_back();
            }
            // When evaluating 2 + (3 * 5), the shunting yard algo turns that
            // into 2 ( id 3 5 * ) +, effectively putting "id" as a dummy
            // function where the algo was expecting a function to call the
            // thing inside the brackets. This reverses that step
            if (fun == "id") {
                fun = args[0].val;
                args = args[0].args;
            }
            oq.push_back(astnode(fun, args2, tok.metadata));
        }
        else oq.push_back(tok);
    }
    // Output must have one argument
    if (oq.size() == 0) {
        std::cerr << "Output blank!\n";
    }
    else if (oq.size() > 1) {
        std::cerr << "Too many tokens in output: ";
        for (int i = 0; i < oq.size(); i++)
            std::cerr << printSimple(oq[i]) << "\n";
    }
    else return oq[0];
}


// Parses one line of serpent
Node parseSerpentTokenStream(std::vector<Node> s) {
    return treefy(shuntingYard(s));
}


// Count spaces at beginning of line
int spaceCount(std::string s) {
    int pos = 0;
    while (pos < s.length() && (s[pos] == ' ' || s[pos] == '\t')) pos += 1;
    return pos;
}

// Is it a bodied command?
bool bodied(std::string tok) {
    return tok == "if" || tok == "elif";
}

// Are the two commands meant to continue each other? 
bool bodiedContinued(std::string prev, std::string tok) {
    return prev == "if" && tok == "elif" 
        || prev == "elif" && tok == "else"
        || prev == "elif" && tok == "elif"
        || prev == "if" && tok == "else"
        || prev == "init" && tok == "code"
        || prev == "shared" && tok == "init";
}

// Parse lines of serpent (helper function)
Node parseLines(std::vector<std::string> lines, Metadata metadata, int sp) {
    std::vector<Node> o;
    int origLine = metadata.ln;
    int i = 0;
    while (i < lines.size()) {
        std::string main = lines[i];
        int spaces = spaceCount(main);
        if (spaces != sp) {
            std::cerr << "Indent mismatch on line " << (origLine+i) << "\n";
        }
        int lineIndex = i;
        metadata.ln = origLine + i; 
        // Tokenize current line
        std::vector<Node> tokens = tokenize(main.substr(sp), metadata);
        // Remove extraneous tokens, including if / elif
        std::vector<Node> tokens2;
        for (int j = 0; j < tokens.size(); j++) {
            if (tokens[j].val == "#" || tokens[j].val == "//") break;
            if (j == tokens.size() - 1 && tokens[j].val == ":") break;
            if (j >= 1 || !bodied(tokens[j].val)) {
                tokens2.push_back(tokens[j]);
            }
        }
        // Is line empty or comment-only?
        if (tokens2.size() == 0) {
            i += 1;
            continue;
        }
        // Parse current line
        Node out = parseSerpentTokenStream(tokens2);
        // Parse child block
        int indent = 999999;
        std::vector<std::string> childBlock;
        while (1) {
            i += 1;
            if (i >= lines.size() || spaceCount(lines[i]) <= sp) break;
            childBlock.push_back(lines[i]);
        }
        // Bring back if / elif into AST
        if (bodied(tokens[0].val)) {
            std::vector<Node> args;
            args.push_back(out);
            out = astnode(tokens[0].val, args, out.metadata);
        }
        // Add child block to AST
        if (childBlock.size()) {
            int childSpaces = spaceCount(childBlock[0]);
            out.type = ASTNODE;
            out.args.push_back(parseLines(childBlock, metadata, childSpaces));
        }
        if (o.size() == 0 || o.back().type == TOKEN) {
            o.push_back(out);
            continue;
        }
        // This is a little complicated. Basically, the idea here is to build
        // constructions like [if [< x 5] [a] [elif [< x 10] [b] [else [c]]]]
        std::vector<Node> u;
        u.push_back(o.back());
        if (bodiedContinued(o.back().val, out.val)) {
            while (bodiedContinued(u.back().val, out.val)) {
                u.push_back(u.back().args.back());
            }
            u.pop_back();
            u.back().args.push_back(out);
            while (u.size() > 1) {
                Node v = u.back();
                u.pop_back();
                u.back().args.pop_back();
                u.back().args.push_back(v);
            }
            o.pop_back();
            o.push_back(u[0]);
        }
        else o.push_back(out);
    }
    if (o.size() == 1) return o[0];
    else return astnode("seq", o, o[0].metadata);
}

// Parses serpent code
Node parseSerpent(std::string s, std::string file) {
    return parseLines(splitLines(s), metadata(file, 0, 0), 0);
}


using namespace std;