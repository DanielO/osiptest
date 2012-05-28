OSIP_CFLAGS=	$(shell pkg-config --cflags libosip2)
OSIP_LIBS=	$(shell pkg-config --libs libosip2)

DEFS=

SRCS=	main.c
OBJS=	$(SRCS:.c=.o)

CFLAGS=		$(DEFS) $(OSIP_CFLAGS) -Wall
LDFLAGS=	$(OSIP_LIBS)

all: osiptest

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

osiptest: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

