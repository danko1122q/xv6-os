#include "character.h"
#include "fcntl.h"
#include "gui.h"
#include "memlayout.h"
#include "msg.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"

char *GUI_programs[] = {"terminal", "editor", "explorer", "floppybird"};

// Pipe for shell communication
int sh2gui_fd[2];
#define READBUFFERSIZE 2000
char read_buf[READBUFFERSIZE];

window programWindow;
int commandWidgetId;
int totallines = 0;
int inputOffset = 25;

// Modern color scheme
struct RGBA bgColor;
struct RGBA promptColor;
struct RGBA textColor;
struct RGBA outputColor;

// Command structures
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5
#define MAXARGS 10

struct cmd {
	int type;
};

struct execcmd {
	int type;
	char *argv[MAXARGS];
	char *eargv[MAXARGS];
};

struct redircmd {
	int type;
	struct cmd *cmd;
	char *file;
	char *efile;
	int mode;
	int fd;
};

struct pipecmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

struct listcmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

struct backcmd {
	int type;
	struct cmd *cmd;
};

// Function declarations
int fork1(void);
void panic(char *);
struct cmd *parsecmd(char *);

// Execute command
void runcmd(struct cmd *cmd) {
	int p[2];
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct listcmd *lcmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;

	if (cmd == 0)
		exit();

	switch (cmd->type) {
	default:
		panic("runcmd");

	case EXEC:
		ecmd = (struct execcmd *)cmd;
		if (ecmd->argv[0] == 0)
			exit();
		exec(ecmd->argv[0], ecmd->argv);
		printf(2, "exec %s failed\n", ecmd->argv[0]);
		break;

	case REDIR:
		rcmd = (struct redircmd *)cmd;
		close(rcmd->fd);
		if (open(rcmd->file, rcmd->mode) < 0) {
			printf(2, "open %s failed\n", rcmd->file);
			exit();
		}
		runcmd(rcmd->cmd);
		break;

	case LIST:
		lcmd = (struct listcmd *)cmd;
		if (fork1() == 0)
			runcmd(lcmd->left);
		wait();
		runcmd(lcmd->right);
		break;

	case PIPE:
		pcmd = (struct pipecmd *)cmd;
		if (pipe(p) < 0)
			panic("pipe");
		if (fork1() == 0) {
			close(1);
			dup(p[1]);
			close(p[0]);
			close(p[1]);
			runcmd(pcmd->left);
		}
		if (fork1() == 0) {
			close(0);
			dup(p[0]);
			close(p[0]);
			close(p[1]);
			runcmd(pcmd->right);
		}
		close(p[0]);
		close(p[1]);
		wait();
		wait();
		break;

	case BACK:
		bcmd = (struct backcmd *)cmd;
		if (fork1() == 0)
			runcmd(bcmd->cmd);
		break;
	}
	exit();
}

// Check if command is a GUI program
int isGUIProgram(char *cmd) {
	int i;
	for (i = 0; i < 4; i++) {
		if (strcmp(cmd, GUI_programs[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

// Input handler with modern prompt
void inputHandler(Widget *w, message *msg) {
	int width;
	int height;
	int charCount;

	if (!w || !w->context.inputfield)
		return;

	width = w->position.xmax - w->position.xmin;
	height = w->position.ymax - w->position.ymin;
	charCount = strlen(w->context.inputfield->text);

	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		inputMouseLeftClickHandler(w, msg);
	} else if (msg->msg_type == M_KEY_DOWN) {
		int c = msg->params[0];
		char buffer[MAX_LONG_STRLEN];

		if (c == '\n' && charCount > 0) {
			memset(read_buf, 0, READBUFFERSIZE);
			memset(buffer, 0, MAX_LONG_STRLEN);

			// Get only the command text (skip the prompt part)
			const char *promptText = "host@xv6os$ ";
			int promptLen = strlen(promptText);
			char *inputText = w->context.inputfield->text;

			// Check if text starts with prompt (manual comparison)
			int hasPrompt = 1;
			int i;
			for (i = 0; i < promptLen; i++) {
				if (inputText[i] != promptText[i]) {
					hasPrompt = 0;
					break;
				}
			}

			if (hasPrompt) {
				strcpy(buffer, inputText + promptLen);
			} else {
				strcpy(buffer, inputText);
			}

			// Skip if command is empty
			if (strlen(buffer) == 0) {
				return;
			}

			// Check if it's a GUI program
			int isGUI = isGUIProgram(buffer);

			if (!isGUI) {
				// Create pipe for non-GUI programs
				if (pipe(sh2gui_fd) != 0) {
					printf(2, "pipe() failed\n");
					// Continue without pipe
					sh2gui_fd[0] = -1;
					sh2gui_fd[1] = -1;
				}
			}

			int pid = fork();
			if (pid < 0) {
				printf(2, "fork failed\n");
				if (sh2gui_fd[0] >= 0) {
					close(sh2gui_fd[0]);
					close(sh2gui_fd[1]);
				}
			} else if (pid == 0) {
				// Child process
				if (!isGUI && sh2gui_fd[1] >= 0) {
					close(sh2gui_fd[0]);
					close(1);
					dup(sh2gui_fd[1]);
					close(sh2gui_fd[1]);
				}
				runcmd(parsecmd(buffer));
				exit(); // Ensure child exits
			} else {
				// Parent process
				if (!isGUI && sh2gui_fd[0] >= 0) {
					close(sh2gui_fd[1]);

					// Read output with timeout mechanism
					int n;
					int totalRead = 0;
					char tempBuf[256];

					// Read in chunks to avoid blocking
					while (totalRead <
					       READBUFFERSIZE - 256) {
						n = read(sh2gui_fd[0], tempBuf,
							 255);
						if (n <= 0)
							break;

						tempBuf[n] = '\0';
						if (totalRead + n <
						    READBUFFERSIZE - 1) {
							strcpy(read_buf +
								       totalRead,
							       tempBuf);
							totalRead += n;
						} else {
							break;
						}
					}
					read_buf[totalRead] = '\0';

					close(sh2gui_fd[0]);
				}

				// Wait for child to finish
				wait();
			}

			// Display command with prompt (separated)
			int commandLineCount;

			// Remove old input widget
			removeWidget(&programWindow, commandWidgetId);

			// Add prompt in green
			addTextWidget(&programWindow, promptColor,
				      "host@xv6os$ ", inputOffset,
				      inputOffset +
					      totallines * CHARACTER_HEIGHT,
				      width - inputOffset * 2, CHARACTER_HEIGHT,
				      1, emptyHandler);

			// Add command text in white on same line
			commandLineCount =
				getMouseYFromOffset(buffer, width - 100,
						    strlen(buffer)) +
				1;
			addTextWidget(&programWindow, textColor, buffer,
				      inputOffset + 100,
				      inputOffset +
					      totallines * CHARACTER_HEIGHT,
				      width - inputOffset * 2 - 100,
				      commandLineCount * CHARACTER_HEIGHT, 1,
				      emptyHandler);

			totallines += commandLineCount;

			// Add command output if exists
			if (strlen(read_buf) > 0) {
				int respondLineCount =
					getMouseYFromOffset(read_buf, width,
							    strlen(read_buf)) +
					1;
				addTextWidget(
					&programWindow, outputColor, read_buf,
					inputOffset,
					inputOffset +
						totallines * CHARACTER_HEIGHT,
					width - inputOffset * 2,
					respondLineCount * CHARACTER_HEIGHT, 1,
					emptyHandler);
				totallines += respondLineCount;
			}

			// Add new input field - PERBAIKAN: empty string untuk
			// text field
			commandWidgetId = addInputFieldWidget(
				&programWindow, promptColor, "", inputOffset,
				inputOffset + totallines * CHARACTER_HEIGHT,
				width - inputOffset * 2, CHARACTER_HEIGHT, 1,
				inputHandler);

			// Auto-scroll to bottom
			int maximumOffset =
				getScrollableTotalHeight(&programWindow) -
				programWindow.height;
			if (maximumOffset > 0) {
				programWindow.scrollOffsetY = maximumOffset;
			}

			programWindow.needsRepaint = 1;
		} else {
			inputFieldKeyHandler(w, msg);

			// Grow input field height as needed
			int newHeight =
				CHARACTER_HEIGHT *
				(getMouseYFromOffset(
					 w->context.inputfield->text, width,
					 strlen(w->context.inputfield->text)) +
				 1);
			if (newHeight > height) {
				w->position.ymax = w->position.ymin + newHeight;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	int width;

	(void)argc;
	(void)argv;

	// Window configuration
	programWindow.width = 600;
	programWindow.height = 450;
	programWindow.hasTitleBar = 1;
	createWindow(&programWindow, "Terminal");

	width = programWindow.width - inputOffset * 2;

	// Modern dark theme colors
	bgColor.R = 30;
	bgColor.G = 30;
	bgColor.B = 30;
	bgColor.A = 255;

	promptColor.R = 76;
	promptColor.G = 175;
	promptColor.B = 80;
	promptColor.A = 255;

	textColor.R = 240;
	textColor.G = 240;
	textColor.B = 240;
	textColor.A = 255;

	outputColor.R = 200;
	outputColor.G = 200;
	outputColor.B = 200;
	outputColor.A = 255;

	// Background
	addColorFillWidget(&programWindow, bgColor, 0, 0, programWindow.width,
			   programWindow.height, 0, emptyHandler);

	// Welcome message
	char welcome[] = "xv6 Terminal v1.0\nCommands: ls, cat, mkdir, rm, "
			 "echo, shell, editor, etc.\n";
	int welcomeLines = 3;
	addTextWidget(&programWindow, textColor, welcome, inputOffset,
		      inputOffset, width, welcomeLines * CHARACTER_HEIGHT, 1,
		      emptyHandler);
	totallines = welcomeLines;

	// PERBAIKAN: Tambah text widget untuk prompt static
	addTextWidget(&programWindow, promptColor, "host@xv6os$ ", inputOffset,
		      inputOffset + totallines * CHARACTER_HEIGHT, width,
		      CHARACTER_HEIGHT, 1, emptyHandler);

	// Initial input field - PERBAIKAN: empty string, prompt ditampilkan
	// oleh text widget di atas
	commandWidgetId = addInputFieldWidget(
		&programWindow, textColor, "",
		inputOffset + 100, // offset untuk posisi setelah prompt
		inputOffset + totallines * CHARACTER_HEIGHT, width - 100,
		CHARACTER_HEIGHT, 1, inputHandler);

	printf(1, "Terminal started\n");

	while (1) {
		updateWindow(&programWindow);
	}

	return 0;
}

void panic(char *s) {
	printf(2, "%s\n", s);
	exit();
}

int fork1(void) {
	int pid;
	pid = fork();
	if (pid == -1)
		panic("fork");
	return pid;
}

// Command constructors
struct cmd *execcmd(void) {
	struct execcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = EXEC;
	return (struct cmd *)cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, char *efile, int mode,
		     int fd) {
	struct redircmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = REDIR;
	cmd->cmd = subcmd;
	cmd->file = file;
	cmd->efile = efile;
	cmd->mode = mode;
	cmd->fd = fd;
	return (struct cmd *)cmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right) {
	struct pipecmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = PIPE;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd *)cmd;
}

struct cmd *listcmd(struct cmd *left, struct cmd *right) {
	struct listcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = LIST;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd *)cmd;
}

struct cmd *backcmd(struct cmd *subcmd) {
	struct backcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = BACK;
	cmd->cmd = subcmd;
	return (struct cmd *)cmd;
}

// Parsing
char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq) {
	char *s;
	int ret;

	s = *ps;
	while (s < es && strchr(whitespace, *s))
		s++;
	if (q)
		*q = s;
	ret = *s;
	switch (*s) {
	case 0:
		break;
	case '|':
	case '(':
	case ')':
	case ';':
	case '&':
	case '<':
		s++;
		break;
	case '>':
		s++;
		if (*s == '>') {
			ret = '+';
			s++;
		}
		break;
	default:
		ret = 'a';
		while (s < es && !strchr(whitespace, *s) &&
		       !strchr(symbols, *s))
			s++;
		break;
	}
	if (eq)
		*eq = s;

	while (s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return ret;
}

int peek(char **ps, char *es, char *toks) {
	char *s;
	s = *ps;
	while (s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *parsecmd(char *s) {
	char *es;
	struct cmd *cmd;

	es = s + strlen(s);
	cmd = parseline(&s, es);
	peek(&s, es, "");
	if (s != es) {
		printf(2, "leftovers: %s\n", s);
		panic("syntax");
	}
	nulterminate(cmd);
	return cmd;
}

struct cmd *parseline(char **ps, char *es) {
	struct cmd *cmd;
	cmd = parsepipe(ps, es);
	while (peek(ps, es, "&")) {
		gettoken(ps, es, 0, 0);
		cmd = backcmd(cmd);
	}
	if (peek(ps, es, ";")) {
		gettoken(ps, es, 0, 0);
		cmd = listcmd(cmd, parseline(ps, es));
	}
	return cmd;
}

struct cmd *parsepipe(char **ps, char *es) {
	struct cmd *cmd;
	cmd = parseexec(ps, es);
	if (peek(ps, es, "|")) {
		gettoken(ps, es, 0, 0);
		cmd = pipecmd(cmd, parsepipe(ps, es));
	}
	return cmd;
}

struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es) {
	int tok;
	char *q, *eq;

	while (peek(ps, es, "<>")) {
		tok = gettoken(ps, es, 0, 0);
		if (gettoken(ps, es, &q, &eq) != 'a')
			panic("missing file for redirection");
		switch (tok) {
		case '<':
			cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
			break;
		case '>':
			cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
			break;
		case '+':
			cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
			break;
		}
	}
	return cmd;
}

struct cmd *parseblock(char **ps, char *es) {
	struct cmd *cmd;
	if (!peek(ps, es, "("))
		panic("parseblock");
	gettoken(ps, es, 0, 0);
	cmd = parseline(ps, es);
	if (!peek(ps, es, ")"))
		panic("syntax - missing )");
	gettoken(ps, es, 0, 0);
	cmd = parseredirs(cmd, ps, es);
	return cmd;
}

struct cmd *parseexec(char **ps, char *es) {
	char *q, *eq;
	int tok, argc;
	struct execcmd *cmd;
	struct cmd *ret;

	if (peek(ps, es, "("))
		return parseblock(ps, es);

	ret = execcmd();
	cmd = (struct execcmd *)ret;

	argc = 0;
	ret = parseredirs(ret, ps, es);
	while (!peek(ps, es, "|)&;")) {
		if ((tok = gettoken(ps, es, &q, &eq)) == 0)
			break;
		if (tok != 'a')
			panic("syntax");
		cmd->argv[argc] = q;
		cmd->eargv[argc] = eq;
		argc++;
		if (argc >= MAXARGS)
			panic("too many args");
		ret = parseredirs(ret, ps, es);
	}
	cmd->argv[argc] = 0;
	cmd->eargv[argc] = 0;
	return ret;
}

struct cmd *nulterminate(struct cmd *cmd) {
	int i;
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct listcmd *lcmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;

	if (cmd == 0)
		return 0;

	switch (cmd->type) {
	case EXEC:
		ecmd = (struct execcmd *)cmd;
		for (i = 0; ecmd->argv[i]; i++)
			*ecmd->eargv[i] = 0;
		break;

	case REDIR:
		rcmd = (struct redircmd *)cmd;
		nulterminate(rcmd->cmd);
		*rcmd->efile = 0;
		break;

	case PIPE:
		pcmd = (struct pipecmd *)cmd;
		nulterminate(pcmd->left);
		nulterminate(pcmd->right);
		break;

	case LIST:
		lcmd = (struct listcmd *)cmd;
		nulterminate(lcmd->left);
		nulterminate(lcmd->right);
		break;

	case BACK:
		bcmd = (struct backcmd *)cmd;
		nulterminate(bcmd->cmd);
		break;
	}
	return cmd;
}