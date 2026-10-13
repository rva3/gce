#pragma once
#define ECHO 0x08
#define ICANON 0x02
#define TCSANOW 0
struct termios {
    unsigned int c_lflag;
};
int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
void cfmakeraw(struct termios *termios_p);
