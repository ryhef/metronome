struct Timeattr_s;
#define IOFUNC_ATTR_T struct Timeattr_s
struct Timeocb_s;
#define IOFUNC_OCB_T struct Timeocb_s

#include <stdio.h>
#include <time.h>
#include <sys/netmgr.h>
#include <sys/neutrino.h>
#include <stdlib.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>


#define PULSE_CODE _PULSE_CODE_MINAVAIL
#define PAUSE_CODE (PULSE_CODE + 1)
#define QUIT_CODE (PAUSE_CODE + 1)
#define START_CODE (QUIT_CODE + 1)
#define STOP_CODE (START_CODE + 1)
#define SET_CODE (STOP_CODE + 1)

#define ATTACH_POINT "metronome"
#define ATTACH_POINT2 "metronome-help"
//second

#define NumDevices 2
#define metronome 0
#define metronomehelp 1

IOFUNC_OCB_T* time_ocb_calloc(resmgr_context_t *ctp, IOFUNC_ATTR_T *tattr);
void time_ocb_free(IOFUNC_OCB_T *tocb);

char *devnames[NumDevices] = {
		"/dev/local/metronome",
		"/dev/local/metronome-help",
};


typedef struct Timeattr_s {
	iofunc_attr_t attr;
	int device;
} Timeattr_t;


typedef struct Timeocb_s {
	iofunc_ocb_t ocb;
	//Here is were you can declare user variables.
	char buffer[400];
} Timeocb_t;



Timeattr_t timeattrs[NumDevices];
//second

char data[255];
int server_coid;
int bpm = 0;
int tsTop = 0;
int tsBot = 0;

name_attach_t * attach;

typedef union {
	struct _pulse pulse;
	char msg[255];
} message_t;

struct DataTableRow
{
	int tsTop;
	int tsBot;
	int numofIntervals;
	char Pattern[32];
};

struct DataTableRow t[] = {
		{2, 4, 4, "|1&2&"},
		{3, 4, 6, "|1&2&3&"},
		{4, 4, 8, "|1&2&3&4&"},
		{5, 4, 10, "|1&2&3&4-5-"},
		{3, 8, 6, "|1-2-3-"},
		{6, 8, 6, "|1&a2&a"},
		{9, 8, 9, "|1&a2&a3&a"},
		{12, 8, 12, "|1&a2&a3&a4&a"}
};

typedef struct bpmTS {
	int bpm;
	int tsTop;
	int tsBot;
}bpmTS ;

void *metronomeThread(void *arg){
	struct sigevent event;
	struct itimerspec itime;
	struct itimerspec itimePrevious;
	timer_t timer_id;
	int rcvid;
	message_t msg;

	bpmTS * arguments = (bpmTS*) arg;

	event.sigev_notify = SIGEV_PULSE;
	event.sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0, attach->chid, _NTO_SIDE_CHANNEL, 0);
	event.sigev_priority = SchedGet(0,0,NULL);
	event.sigev_code = PULSE_CODE;

	timer_create(CLOCK_REALTIME, &event, &timer_id);

	bpm = arguments->bpm;
	tsTop = arguments->tsTop;
	tsBot = arguments->tsBot;

	double beat =(double) 60 / bpm;
	double perMeasure = beat * 2;
	double perInterval =  (double) perMeasure  / tsBot;
	double fractional = perInterval - (int) perInterval;


	itime.it_value.tv_sec = perInterval;
	itime.it_value.tv_nsec = (fractional * 1e+9);

	itime.it_interval.tv_sec = perInterval;
	itime.it_interval.tv_nsec = (fractional * 1e+9);

	timer_settime(timer_id, 0, &itime, NULL);

	int index = -1;
	for(int i = 0; i < 8; ++i)
		if (t[i].tsTop == arguments->tsTop)
			if (t[i].tsBot == arguments->tsBot)
				index = i;

	if(index == -1){
		printf("Incorrect pattern, metronome failed");
		exit(EXIT_FAILURE);
	}

	for (;;){

		char *ptr;
		ptr=t[index].Pattern;

		while(1){

			rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
			if (rcvid == 0){
				if (msg.pulse.code == PULSE_CODE){
					if(*ptr == '|'){
						printf("%c1", *ptr++);
						*ptr++;
					}
					else{
						printf("%c", *ptr++);
					}
				}

				else if (msg.pulse.code == PAUSE_CODE){
					itime.it_value.tv_sec = msg.pulse.value.sival_int;
					timer_settime(timer_id, 0, &itime, NULL);
				}
				else if (msg.pulse.code == START_CODE){
					if((itime.it_value.tv_sec == 0) && (itime.it_value.tv_nsec == 0)){
						//itime.it_value.tv_sec = msg.pulse.value.sival_int;
						itime.it_value.tv_sec = itimePrevious.it_value.tv_sec;
						itime.it_value.tv_nsec = itimePrevious.it_value.tv_nsec;
						timer_settime(timer_id, 0, &itime, NULL);
					}
				}
				else if (msg.pulse.code == STOP_CODE){
					if(!((itime.it_value.tv_sec == 0) && (itime.it_value.tv_nsec == 0))){
						itime.it_value.tv_sec = 0;
						itime.it_value.tv_nsec = 0;
						timer_settime(timer_id, 0, &itime, &itimePrevious);
					}
				}
				else if (msg.pulse.code == SET_CODE){
					double beat =(double) 60 / bpm;
					double perMeasure = beat * 2;
					double perInterval =  (double) perMeasure  / tsBot;
					double fractional = perInterval - (int) perInterval;

					timer_settime(timer_id, 0, &itime, NULL);
					if(!((itime.it_value.tv_sec == 0) && (itime.it_value.tv_nsec == 0))){
						itimePrevious.it_value.tv_sec = 1;
						itimePrevious.it_value.tv_nsec = 500000000;

						itimePrevious.it_interval.tv_sec = perInterval;
						itimePrevious.it_interval.tv_nsec = (fractional * 1e+9);
						timer_settime(timer_id, 0, &itimePrevious, NULL);
					}
					else{
						itime.it_value.tv_sec = 1;
						itime.it_value.tv_nsec = 500000000;

						itime.it_interval.tv_sec = perInterval;
						itime.it_interval.tv_nsec = (fractional * 1e+9);
						timer_settime(timer_id, 0, &itime, NULL);
					}
					index = msg.pulse.value.sival_int;
				}
				else if (msg.pulse.code == QUIT_CODE){
					printf("\nQuiting\n");

					TimerDestroy(timer_id);
					exit(EXIT_SUCCESS);
				}
				if(*ptr == '\0'){
					printf("\n");
					break;
				}
			}
			fflush( stdout );
		}
	}
	return EXIT_SUCCESS;
}

int io_read(resmgr_context_t *ctp, io_read_t *msg, Timeocb_t *tocb)
{
	int nb;


	double beat =(double) 60 / bpm;
	double perMeasure = beat * 2;
	double perInterval =  (double) perMeasure  / tsBot;
	double fractional = (perInterval - (int) perInterval)* 1e+9;



	if (tocb->ocb.attr->device == metronome) {
		sprintf(tocb->buffer, "[metronome: %d beats/min, time signature %d/%d, sec-per-beat: %.2f, nanoSecs: %d] \n", bpm, tsTop, tsBot, perInterval, (int)fractional);
	} else if (tocb->ocb.attr->device == metronomehelp) {
		sprintf(tocb->buffer,"Metronome Resource Manager (ResMgr)\n"
		"Usage: metronome <bpm> <ts-top> <ts-bottom>\n"
		"API:\n"
		"pause [1-9] - pause the metronome for 1-9 seconds\n"
		"quit - quit the metronome\n"
		"set <bpm> <ts-top> <ts-bottom> - set the metronome to <bpm> ts-top/ts-bottom\n"
		"start - start the metronome from stopped state\n"
		"stop - stop the metronome; use 'start' to resume\n");
	}

	nb = strlen(tocb->buffer);

	if (tocb->buffer == NULL)
		return 0;

	if (tocb->ocb.offset == nb)
		return 0;
	nb = min(nb, msg->i.nbytes);
	_IO_SET_READ_NBYTES(ctp, nb);

	SETIOV(ctp->iov, tocb->buffer, nb);
	tocb->ocb.offset += nb;

	if (nb > 0)
		tocb->ocb.attr->attr.flags |= IOFUNC_ATTR_ATIME;

	return (_RESMGR_NPARTS(1));
}

int io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb)
{
	int nb = 0;
	if (ocb->ocb.attr->device == metronomehelp) {
		printf("Cannot write metronome-help\n");

		nb = msg->i.nbytes;
	} else if (ocb->ocb.attr->device == metronome) {


		if (msg->i.nbytes == ctp->info.msglen - (ctp->offset + sizeof(*msg))){
			char *buf;
			char *alert_msg;
			int i, small_integer;
			struct bpmTS testinputs;
			buf = (char *)(msg + 1);

			if (strstr(buf, "pause") != NULL){
				for (i = 0; i < 2; i++)
					alert_msg = strsep(&buf, " ");

				small_integer = atoi(alert_msg);
				if (small_integer >= 1 && small_integer <= 9)
					MsgSendPulse(server_coid, SchedGet(0, 0, NULL), PAUSE_CODE, small_integer);
				else
					printf("\nInteger is not between 1 and 9.\n");
			}
			else if (strstr(buf, "set") != NULL){
				for (i = 0; i < 4; i++){

					alert_msg = strsep(&buf, " ");

					if(i==1){
						testinputs.bpm = atoi(alert_msg);
					}
					else if(i==2){
						testinputs.tsTop = atoi(alert_msg);
					}

					else if(i==3){
						testinputs.tsBot = atoi(alert_msg);
					}
				}

				int index = -1;
				for(int i = 0; i < 8; ++i)
					if (t[i].tsTop == testinputs.tsTop)
						if (t[i].tsBot == testinputs.tsBot)
							index = i;

				if(index == -1){
					printf("\nError:Incorrect time signature pattern\n");
				}else{
					bpm = testinputs.bpm;
					tsTop = testinputs.tsTop;
					tsBot = testinputs.tsBot;
					MsgSendPulse(server_coid, SchedGet(0, 0, NULL), SET_CODE, index);
				}
			}
			else if (strstr(buf, "start") != NULL)
				MsgSendPulse(server_coid, SchedGet(0, 0, NULL), START_CODE, 0);
			else if (strstr(buf, "stop") != NULL)
				MsgSendPulse(server_coid, SchedGet(0, 0, NULL), STOP_CODE, 0);
			else if (strstr(buf, "quit") != NULL)
				MsgSendPulse(server_coid, SchedGet(0, 0, NULL), QUIT_CODE, 0);
			else
				printf("\nError - '%s' is not a valid command\n", strsep(&buf, "\n"));

			nb = msg->i.nbytes;
		}
	}
	_IO_SET_WRITE_NBYTES(ctp, nb);

	if (msg->i.nbytes > 0)
		ocb->ocb.attr->attr.flags |= IOFUNC_ATTR_ATIME;

	return (_RESMGR_NPARTS(0));
}

int io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle, void *extra)
{
	if ((server_coid = name_open(ATTACH_POINT, 0)) == -1)
	{
		perror("name_open failed.");
		return EXIT_FAILURE;
	}
	return (iofunc_open_default(ctp, msg, handle, extra));
}

int main(int argc, char *argv[])
{
	dispatch_t* dpp;
	resmgr_io_funcs_t io_funcs;
	resmgr_connect_funcs_t connect_funcs;
	dispatch_context_t   *ctp;
	int i;

	iofunc_funcs_t time_ocb_funcs = {
			_IOFUNC_NFUNCS,
			time_ocb_calloc,
			time_ocb_free
	};

	iofunc_mount_t time_mount = { 0, 0, 0, 0, &time_ocb_funcs };


	attach = name_attach(NULL, ATTACH_POINT, 0);

	if(attach == NULL){
		perror("failed to create the channel.");
		exit(EXIT_FAILURE);
	}

	if (argc != 4){
		perror("Not the correct arguments. Usage is metronome <beats-per-minute> <time-signature-top> <time-signature-bottom>");
		exit(EXIT_FAILURE);
	}

	struct bpmTS arguments = {
			atoi(argv[1]),
			atoi(argv[2]),
			atoi(argv[3]),
	};

	dpp = dispatch_create();
		iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);


	connect_funcs.open = io_open;
	io_funcs.read = io_read;
	io_funcs.write = io_write;

	for(i = 0; i < NumDevices; i++) {
		   iofunc_attr_init (&timeattrs[i].attr, S_IFCHR | 0666, NULL, NULL);
		   timeattrs[i].device = i;
		   timeattrs[i].attr.mount = &time_mount;
		   resmgr_attach(dpp, NULL, devnames[i], _FTYPE_ANY, NULL, &connect_funcs, &io_funcs, &timeattrs[i]);
		}

		ctp = dispatch_context_alloc(dpp);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(NULL, &attr, &metronomeThread, &arguments);
	while(1) {
		ctp = dispatch_block(ctp);
		dispatch_handler(ctp);
	}
	return EXIT_SUCCESS;
}

Timeocb_t * time_ocb_calloc(resmgr_context_t *ctp, Timeattr_t *tattr) {
	Timeocb_t *tocb;
	tocb = calloc(1, sizeof(Timeocb_t));
	tocb->ocb.offset = 0;
	return(tocb);
}

void time_ocb_free(Timeocb_t *tocb) {
	free(tocb);
}
