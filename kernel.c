#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

/* current cursor location */
unsigned int current_loc = 0;
/* video memory begins at address 0xb8000 */
char *vidptr = (char*)0xb8000;

struct IDT_entry {
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

int isloggedin = 0;
char* keystring = '\0';

void idt_init()
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	/*     Ports
	*	 PIC1	PIC2
	*Command 0x20	0xA0
	*Data	 0x21	0xA1
	*/

	/* ICW1 - begin initialization */
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);

	/* ICW2 - remap offset address of IDT */
	/*
	* In x86 protected mode, we have to remap the PICs beyond 0x20 because
	* Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
	*/
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);

	/* ICW3 - setup cascading */
	write_port(0x21 , 0x00);
	write_port(0xA1 , 0x00);

	/* ICW4 - environment info */
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);
	/* Initialization finished */

	/* mask interrupts */
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);

	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}

void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xFD);
}

void kprint(const char *str)
{
	unsigned int i = 0;
	while (str[i] != '\0') {
		vidptr[current_loc++] = str[i++];
		vidptr[current_loc++] = 0x07;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE) {
		vidptr[i++] = ' ';
		vidptr[i++] = 0x07;
		current_loc = 0;
	}
}

int stringlength (char *str) {
    int len = 0;
    while (*str != '\0') {
        str++;
        len++;
    }
    return len;
}

int stringcomp(char *a, char *b)
{
    return (*a == *b && *b == '\0')? 0 : (*a == *b)? stringcomp(++a, ++b): 1;
}

char* append(char* s, char c) {
        int len = stringlength(s);
        s[len] = c;
        s[len+1] = '\0';
	return s;
}

void handlecommand(void)
{
	if ((stringcomp(keystring, "pass") == 0) && isloggedin == 0)
	{
		isloggedin = 1;
		kprint("You have now logged in!");
		kprint_newline();
		kprint("To open the help menu, type 'help'.");
	}
	else if (isloggedin == 0)
	{
		kprint("Error! Please try again:");
	}
	else
	{
		if (stringcomp(keystring, "help") == 0)
		{
			kprint("Clear Screen: Alt");
			kprint_newline();
			kprint("Log out: exit");
		}
		else if (stringcomp(keystring, "exit") == 0)
		{
			clear_screen();
			isloggedin = 0;
			kprint("Please enter your password:");
		}
		else
		{
			kprint("That command was not recognised! Please try again, or type 'help' for the help menu.");
		}
	}
	*keystring = '\0';
}

int handlekeycode(char keycode)
{

	if(keycode == 0x1C)
	{
		kprint_newline();
		kprint_newline();
		handlecommand();
		kprint_newline();
		kprint_newline();
		kprint("OPCC-OS > ");
		return 1;
	}

	else if(keycode == 0x38)
	{
		clear_screen();

		kprint("OPCC-OS > ");
		return 1;
	}

	return 0;
}

void keyboard_handler_main(void)
{
	unsigned char status;
	char keycode;

	/* write EOI */
	write_port(0x20, 0x20);

	status = read_port(KEYBOARD_STATUS_PORT);
	/* Lowest bit of status will be set if buffer is not empty */
	if (status & 0x01) {
		keycode = read_port(KEYBOARD_DATA_PORT);

		if(handlekeycode(keycode) == 1)
		{
			return;
		}

		if(keycode < 0)
		{
			return;
		}

		keystring = append(keystring, keyboard_map[(unsigned char) keycode]);
		vidptr[current_loc++] = keyboard_map[(unsigned char) keycode];
		vidptr[current_loc++] = 0x07;
	}
}

void kmain(void)
{
	clear_screen();
	kprint("Welcome to OPCC-OS!");
	kprint_newline();
	kprint("Please enter your password:");
	kprint_newline();
	kprint("OPCC-OS > ");
	keystring = "";

	idt_init();
	kb_init();

	while(1);
}
