## CONTENT IN THIS REPO

This repo adds a simple GUI interface to the xv6 system (the x86 version by October 2020). It is a single person final project for the course CS510 offered by Duke University.

## ACKNOWLEDGMENTS

Please see [xv6 public repo](https://github.com/mit-pdos/xv6-public) for the acknowledgments of the public repo. For this repo, it borrows code from 
[Themis_GUI](https://github.com/YueDayu/Themis_GUI) (characters.c, gui.c, mouse.c, kbd.c, msg.c). 

## BUILDING AND RUNNING XV6

To build xv6 on an x86 ELF machine (like Linux or FreeBSD), run "make". On non-x86 or non-ELF machines (like OS X, even on x86), you will need to install a cross-compiler gcc suite capable of producing x86 ELF binaries (see https://pdos.csail.mit.edu/6.828/). Then run "make TOOLPREFIX=i386-jos-elf-". Now install the QEMU PC simulator and run "make qemu".

## GUI RESULT

This is what the GUI interface looks like when you have successfully booted the system. Explorer the interface by clicking on buttons, icons, or even text you've typed. Or use your keyboard to type something in the shell or the editor (support most single keys including enter, backspace and arrow keys).  

<img src="/pics/multiple_window.png" width="600">

<img src="/pics/shell.png" width="600">

<img src="/pics/flappy_bird.png" width="600">

## GUI ARCHITECTURE

### Kernel level side:

We first introduce a struct called kernel window which remembers its position (a bounded rectangle with xmin, xmax, ymin, ymax), whether it is minimized or not, etc. It also keeps the pointer to the content of the window (a 2D array of RGB values).

The kernel part of the GUI code maintains a doubly linked list of kernel windows (called windowlist). Those windows are the ordinary windows we see in a GUI operating system. They have a title bar that can be used to dragged the window around, or click on the minimize/close icon to minimize or close this window. They overlap with each other and the top most one is the focused one that interacts with the user. There is also a global single kernel window called the popup window. This window does not have a title bar (so it can not be dragged). It will be dismissed by clicking anywhere outside of it. It is also painted on top of every window in the windowlist. This popup window can be used to implement the start window or the right click popup message window that we see in Windows operating system. It keeps a handle to the window that creates it, which allows it to access the functions and data of its creator. 

The kernel is in charge of handling and dispatching the messages from the keyboard and the mouse. The tail of the windowlist is the focused window. The kernel needs to handle focus change (mouse down on the window), dragging/closing/minimizing the window in the title bar, and dispatch other messages (mouse click, keyboard input) to the focused window. 

The only function in the kernel that updates(redraws) the screen is the updateScreen() function which will only be called by the desktop program. This function first clears the screen buffer, draw the desktop window, draw the dock, draw other windows in order, draw the systemwide popup window, then draws the mouse. Then it flushes the screen buffer to the screen (the real video memory). This may cause extra computational cost, but greatly simplifies the coding. 

### User level side:

At the user side, there is a struct called window that every GUI user program maintains. This window communicates with its corresponding kernel window about settings and messages. The window has no idea where it is on the screen or whether it is minimized or not. Those information are only kept in the kernel. 

The window also maintains a doubly linked list of widgets which are all that matters when drawing the window. Widget is a different struct which consists of a widget type, its position relative to the window, and a handler that responds to messages. Currently defined widget types are colorFill (a rectangle that is filled with one color), text(plain text with a certain color), button(a solid color rectangle with some text inside) and inputField (has a cursor that indicates the current offset to the text inside). 

The GUI user program would add the widgets, write message handlers for these widgets (which may create or remove more widgets along the way), and then call updateWindow() in a infinite loop. The updateWindow function is similar to the updateScreen() system call. It redraws all the widgets from bottom to top if needRepaint is true for the window. It also tries to read new messages for the window. If there is new message, it dispatches the messsage to the right widget based on its position and order and sets needRepaint to true. Otherwise the window does not need to be repainted. 

There are some predefined handlers such as the empty handler (does nothing), inputKeyHandler(responds to ascii values includes '\n', arrows keys that modifies the text cursor), inputMouse Handler(responds to mouse click that modifies the text cursor), execHandler(start another process). One can combine these handlers easily or create new ones. 

There are also situations when the total height of the widgets is too long (e.g. for a GUI shell). To deal with such situations, I add another attribute called scrollOffsetY to the window. The widgets can choose whether they are scrollable or not. The scrollable widgets are then painted with the scrollOffsetY subtracted in the y direction. The mouse y position is added by this scrollOffsetY when passing a message to these widgets. In this way we can create the illusion that the window is actually scrolling in the y direction. The x direction scrolling is also added. This y scrolling is currently used in the shell.c and editor.c program to accomondate long text. It is not necessary as we can just move the position of the widgets in interest directly, but is another trick to simpify the coding. 

Currently implemented GUI programs are desktop.c, demo.c, shell.c, editor.c, explorer.c and startWindow.c (totaling of ~1000 lines of code). Other GUI related functions that I implemented takes ~2000 lines of code. The code I borrowed from [Themis_GUI](https://github.com/YueDayu/Themis_GUI) (mouse and keyboard driver, primitive painting functions, definition of macros and some structs) is ~1000 lines.

![Image of GUI arch](/pics/xv6_gui_architecture.png)


## CURRENT POSSIBLE ISSUES

This project is still a work in progress. I couldn't devote more time to it being occupied by my research and other courses. Bugs are to be expected. Here are some of the issues I have found so far:

- The starting address for other devices (DEVSPACE in memlayout.h) has changed which makes me unable to access the video memory using the assembly code from [Themis_GUI](https://github.com/YueDayu/Themis_GUI). I have changed this macro back to its previous value which seems to solve this issue. 
- The file system is somehow reporting errors as I add more code and file into the system. I have increased the block size (BSIZE) from 512 to 1024 which seems to solve the problem. However when adding more files this problem would emerge again and increase BSIZE further to 2048 will fail the system at start up. A rework of the file system is needed.
- The mouse is sometimes restricted to a smaller rectangular region inside the screen. The "solution" is to move the mouse to the boundaries of the screen to calibrate its position. The keyboard driver does not recgonize some key combinations. I currently only consider single key input. 
- When you click the mkdir button in the file explorer to create a temp directory under the root directory, some other directories with random names are also being created. I have no idea how this happens... 
- I can only start other GUI programs if I add a print statement in the current GUI programs message handler. I haven't figured out how to resolve this issue.  

## FUTURE PLAN

Here are some ideas I think can be explored in the future:
- Add functions for drawing line, circle and other graph primitives. We can then implement a simple painting program.
- Redo the widget class. Organize widgets into a tree structure. Add layout widget class (row, column, grid,etc). Add messaging between widgets. Add more constraint options (percentage instead of actual pixels). Basically working towards a modern GUI implementation.
- Add system-wide text selection, copy and paste (through the popup window functionality).
- Add more user programs like image viewer, some simple games. 


