#include <u.h>
#include <libc.h>
#include <bio.h>

void
main(int argc, char **argv)
{
	Biobuf in;
	char *p;
	int fd;
	int n;
	char buf[128];
	char *server;

	if(argc > 1)
		server = argv[1];
	else
		server = "/net/cs";

	Binit(&in, 0, OREAD);
	for(;;close(fd)){
		print("> ");
		p = Brdline(&in, '\n');
		if(p == 0)
			break;
		fd = open(server, ORDWR);
		if(fd < 0)
			exits(server);
		p[Blinelen(&in)-1] = 0;
		if(write(fd, p, strlen(p)) <= 0){
			perror(p);
			continue;
		}
		seek(fd, 0, 0);
		while((n = read(fd, buf, sizeof(buf)-1)) > 0){
			buf[n] = 0;
			print("%s\n", buf);
		}
	}
}
