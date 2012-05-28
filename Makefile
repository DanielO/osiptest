OSIP_CFLAGS=	$(shell pkg-config --cflags libosip2)
OSIP_LIBS=	$(shell pkg-config --libs libosip2)

DEFS=

PROG=	osiptest
SRCS=	main.c
OBJS=	$(SRCS:.c=.o)

CFLAGS=		$(DEFS) $(OSIP_CFLAGS) -Wall -g -O0
LDFLAGS=	$(OSIP_LIBS)

all: $(PROG)

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	$(RM) $(OBJS) $(PROG)
