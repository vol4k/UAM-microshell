#define _GNU_SOURCE // man asprintf
#include <stdio.h> // i/o types
#include <sys/wait.h> // wait
#include <readline/readline.h> // readline strcpy strpbrk strcmp
#include <readline/history.h> // add_history
#include <errno.h> // errno strerror
#include <stdbool.h> // bool
#include <stdlib.h> // malloc free exit
#include <unistd.h> // exec* getcwd fork chdir
#include <pwd.h> // getpwuid
#include <signal.h> // signal

// about info
#define AUTHOR "Vadzim Valchkovich"
#define UID 474137
// compile line: gcc microshell.c -o microshell -lreadline

// default linux varibles
#define MAX_BUFFER_LEN 4048

// esc color codes
#define F_RESET     "\001\033[0m\002"
#define F_BLACK 	"\001\033[1;30m\002"
#define F_RED		"\001\033[1;31m\002"
#define F_GREEN		"\001\033[1;32m\002"
#define F_YELLOW	"\001\033[1;33m\002"
#define F_BLUE		"\001\033[1;34m\002"
#define F_MAGENTA 	"\001\033[1;35m\002"
#define F_CYAN		"\001\033[1;36m\002"
#define F_WHITE		"\001\033[1;37m\002"

// id's of user programs
enum progID {
    prExit, 
    prHelp, 
    prCd, 
    prClear, 
    prSize
    };

// user programs description
#define EXIT_DESCRIPTION "cause the shell to exit"
#define HELP_DESCRIPTION "shows help about a program"
#define CD_DESCRIPTION "change the working directory"
#define CLEAR_DESCRIPTION "clear the terminal screen"

struct uProgram;

// default microshell varibles 
struct args {
    bool loop;
    int exitCode;
    char* shell;
    char** argv;
    int argc;
    struct uProgram *program;
};

// helper varible
pid_t process = 0;

// user programs list
struct uProgram {
    char name[256];
    char description[256];
    void (*run)(struct args*);
};

// helper functions
char* setCollor(char*, char*);
char* next(char*);
char* nextE(char*, char* const);
char* split(char*);
char* join(char*, char*);
char* concQuotes(char*,char* const);
int argsCount(char*, char*);
void buildShellInterface(struct args*);
void ctrlC();
void ctrlZ();

// core functions
void init(struct args*);
void mLoop(struct args*);
void parseCall(struct args*);
void tryCall(struct args*);
void release(struct args*);
bool errd();

// user programs
void uExit(struct args*);
void uHelp(struct args*);
void uCd(struct args*);
void uClear(struct args*);

int main()                                                                          // OK
{
    struct args def;
    init(&def);
    mLoop(&def);

    return def.exitCode;
}


char* setCollor(char* line, char* color)                                            // OK
{
    char* colorline;
    asprintf(&colorline, "%s%s%s", color, line, F_RESET);
    return colorline;
}

char* next(char* iterator)                                                          // OK
{
    return (iterator ? (iterator + strlen(iterator) + 1) : NULL);
}

char* nextlim(char* iterator, char* end)                                            // OK
{
    iterator = next(iterator);
    return (iterator == end ? NULL : iterator);
}

char* split(char* line)                                                             // OK
{
    for(char* iterator = line; iterator = strpbrk(iterator, " \n\t"); iterator++)
        *iterator = '\0';
        
    return line;
}

char* join(char* begin, char* end)                                                  // OK
{
    if(begin && end)
        while((begin + strlen(begin)) != (end + strlen(end)))
            *(begin + strlen(begin)) = ' ';
    return begin;
}

char* concQuotes(char* begin, char* const end)                                      // OK
{
    char *iterator = begin;
    char *quote, *l, *r, *open;
    bool openQuote = false;
    
    while(iterator)
    {
        l = iterator;
        r = l + strlen(iterator) - 1;
        
        if(quote = strchr(iterator, '"'))
        {
            if(quote == l || quote == r) // "{words} || {words}" || "{words}" cases
            {
                if(openQuote) // the quoute is opened already
                {
                    if(*quote == *r && quote == r) // only ...{words}" 
                    {
                        openQuote = false;  // quotation complete
                        *open = *r = '\0';  // removal of quotes
                        iterator = join(open + 1, r); // connect words
                    }
                    else    // any other option is not suitable
                    {
                        errno = EINVAL;
                        iterator = NULL;
                    } 
                }
                else if(*l == *quote) // the quoute isn't opened already and "{words}... case
                {
                    if(l == r) // "{words}" case
                    {
                        openQuote = true;   // quotation opened
                        open = iterator;
                    }
                    else    // check other cases
                    {
                        char *nq = strchr(l + 1, '"');
                        if(!nq) // only "{words}... case
                        {
                            openQuote = true; // quotation opened
                            open = iterator;
                        }
                        else // check other cases
                        {    
                            if(nq == r) // "{words}" case
                            {
                                *l = *r = '\0'; // removal of quotes
                                iterator = join(l + 1, r); // connect words
                            }
                            else    // any other option is not suitable
                            {
                                errno = EINVAL;
                                iterator = NULL;
                            }
                        }   
                    }
                }
                else    // any other option is not suitable
                {
                    errno = EINVAL;
                    iterator = NULL;
                } 
            }
        }

        iterator = nextlim(iterator, end); // go to next word
    }

    if(openQuote) // if quotation isn't complete have an error
    {
        errno = EINVAL;
        iterator = NULL;
    } 

    return (errno ? NULL : (*begin ? begin : next(begin))); // if line start of NULL skip first word 
}

int argsCount(char* iterator, char* end)                                            // OK
{   
    if(!iterator || iterator == end) 
        return 0;

    int counter = 0;

    while(iterator)
    {
        if(iterator && *iterator)   // word exist?
            counter++;
        iterator = nextlim(iterator, end);  // next word
    }
    return counter;
}

void buildShellInterface(struct args* def)                                          // OK
{   
    char *user = setCollor(getpwuid(getuid())->pw_name, F_GREEN);
    char *patch = setCollor(getcwd(NULL, MAX_BUFFER_LEN), F_MAGENTA);

    asprintf(&def->shell, "\r%s[%s:%s]$ ", F_RESET, user, patch); // [{current user}:{path}]$ 

    free(user);
    free(patch);
}

void ctrlC()
{
    kill(process, SIGINT);
}

void ctrlZ()
{
    kill(process, SIGTSTP);
}


void init(struct args* def)                                                         // OK
{
    *def = (struct args){
        .loop = true, 
        .exitCode = 0, 
        .shell = NULL, 
        .argv = NULL, 
        .argc = 0, 
        .program = (struct uProgram*)malloc(sizeof(struct uProgram) * prSize),
        };
    
    def->program[prExit] = (struct uProgram){.name = "exit", .description = EXIT_DESCRIPTION, .run = uExit};
    def->program[prHelp] = (struct uProgram){.name = "help", .description = HELP_DESCRIPTION, .run = uHelp};
    def->program[prCd] = (struct uProgram){.name = "cd", .description = CD_DESCRIPTION, .run = uCd};
    def->program[prClear] = (struct uProgram){.name = "clear", .description = CLEAR_DESCRIPTION, .run = uClear};
}

void mLoop(struct args* def)                                                        // OK
 {
    do{
        buildShellInterface(def);
        parseCall(def);
        tryCall(def);
        release(def);
        errd();
    }while(def->loop);
}

void parseCall(struct args* def)                                                    // OK
{
    if(errno) return;

    char* line;
    char* iterator;
    asprintf(&line, "%s", readline(def->shell)); // get user line
    
    if(line)    // if line not empty 
        add_history(line); // add line to history

    char *end = next(line); // save end pointer of original line
    
    def->argc = argsCount(iterator = concQuotes(split(line), end), end); // convert string to list of arguments

    if(!def->argc) // if list of arguments is empty
    {
        free(line);
        return;
    }

    def->argv = (char**)malloc(sizeof(char*) * (def->argc + 1)); // count of arguments + 1, because execvp() take [{arguments}, NULL]
    
    for(int i = 0; i < def->argc; iterator = nextlim(iterator, end))
    {
        if(iterator && *iterator) // if word exist
        {
            def->argv[i] = (char*)malloc(sizeof(char)*strlen(iterator));
            strcpy(def->argv[i], iterator); // copy
            i++;
        }
    }
    def->argv[def->argc] = NULL; // for execvp()

    free(line);
}

void tryCall(struct args* def)                                                      // OK
{
    if(!def->argc || errno) return;

    for(int i = 0; i < prSize; i++)
        if(!strcmp(def->program[i].name, def->argv[0]))
            {
                def->program[i].run(def);
                return;
            }

    process = fork();

    if(!process)    // child
    {
        execvp(def->argv[0], def->argv);
        exit(errno);
    }
    else if(process > 0)    // parent
    {
        signal(SIGINT,ctrlC);
        signal(SIGTSTP,ctrlZ);
        wait(&process);
        errno = WEXITSTATUS(process);
    }
}

void release(struct args* def)                                                      // OK
{
    free(def->shell);
    for(int i = 0; i < def->argc; i++)
        free(def->argv[i]);
    if(def->argc)
        free(def->argv);
    if(!def->loop)
        free(def->program);
}

bool errd()                                                                         // OK
{
    if(errno)
    {
        printf("%sError (%d): %s\n%s", F_RED, errno, strerror(errno), F_RESET);
        errno = 0;
        return true;
    }
    else
        return false;
}


void uExit(struct args* def)                                                        // OK
{
    if(def->argc > 2) 
    { 
        errno = E2BIG; 
        return; 
    }
    else if (def->argc == 2) 
        def->exitCode = atoi(def->argv[1]);
    def->loop = false;
    errno = 0;
}

void uHelp(struct args* def)                                                        // OK
{    
    if(def->argc > 1) 
    { 
        errno = E2BIG; 
        return; 
    }
    printf("\n");
    printf("%s*** MicroShell SO ***\n", F_MAGENTA);
    printf("%sAuthor:%s\t%s\n", F_BLUE, F_WHITE, AUTHOR);
    printf("%sUID:%s\t%d\n\n", F_BLUE, F_WHITE, UID);
    printf("%sUser programs:\n", F_MAGENTA);
    for(int i = 0; i < prSize; i++)
    {
        printf("%s%s\t", F_BLUE, def->program[i].name);
        printf("%s%s\n", F_WHITE, def->program[i].description);
    }
    printf("%s\n", F_RESET);
}

void uCd(struct args* def)                                                          // OK
{
    if(def->argc > 2) 
    { 
        errno = E2BIG; 
        return; 
    }
    else if (def->argc == 1)    // cd 
        chdir(getpwuid(getuid())->pw_dir);
    else if (!strcmp(def->argv[1], "~"))    // cd ~
        chdir(getpwuid(getuid())->pw_dir);
    else // cd {...}
        chdir(def->argv[1]);
}

void uClear(struct args* def)                                                       // OK
{ 
    if(def->argc > 1) 
    { 
        errno = E2BIG; 
        return; 
    }
    printf ("\033[0d\033[2J");  
    // \033[0d - move the cursor on #d column
    // \033[2J - clean the terminal
}