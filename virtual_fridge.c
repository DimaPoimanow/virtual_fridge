#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
enum {SIGTEMP1 = 10};
enum {SIGTEMP2 = 12};
enum {SIGFREON1 = 16};
enum {SIGFREON2 = 17};
enum {SIGOPENDOOR = 30};
enum {SIGCLOSEDOOR = 31};
enum {TIME_FREON = 4};
enum {TIME_DOOR = 8};
enum {TIME_CAMERA = 3};
enum {TIME_MODUL = 2};
enum {TIME_PEEEEP = 5};
enum {PEEEEP_AMOUNT = 2};
enum {SIGCLOSE = 10};
#define MODUL (5)
#define FREON (2)
#define CAMERA (1)
#define DOOR (3)

int status = 0;
int state = 0;
int alarm_status = 0;
int current_temp = 3;
int fd[2];
int pid1;
int pid[6];
int repeat = 0;

void
current_status(int s){
    printf("----------------------------------------------------\n");
    printf("---------------------INFORMATION--------------------\n");
    for (int i = 0; i < 6; i++){
        kill(pid[i], SIGINT);
    }

}

void
modul_status(int s){
    unsigned int sec = alarm(0);
    printf("------Modul is ON; Next request after %d sec---------\n", sec);
    alarm(sec);
}

void
thermometer_status(int s){
    unsigned int sec = alarm(0);
    printf("----Thermometer is ON; Next request after %d sec-----\n", sec);
    alarm(sec);
}

void
freon_status(int s){
    if (status) {
        printf("-------------------Freon is ON-----------------------\n");
    } else {
        printf("-------------------Freon is OFF----------------------\n");
    }
    repeat = 1;
}

void
air_status(int s){
    status = 0;
    unsigned int sec = alarm(0);
    printf("--------Air TEMP = %d; Heating after %d sec----------\n", current_temp, sec);
    alarm(sec);
}

void
door_status(int s){

    if (status) {
        printf("------------------Door is OPEN----------------------\n");
    } else {
        printf("------------------Door is CLOSED--------------------\n");
    }
    state = 1;
    repeat = 1;

}

void
signalization_status(int s){
    if (status) {
        unsigned int sec = alarm(0);
        printf("----Signalization is ON; Next sound after %d sec----\n", sec);
        alarm(sec);
    } else {
        printf("------------Signalization is OFF--------------------\n");
    }
}

void
main_terminated(int s){
    for (int i = 0; i < 6; i++){
        kill(pid[i], SIGTERM);
    }
    for (int i = 0; i < 6; i++){
        wait(NULL);
    }
    printf("All processes are terminated!\n");
    unlink("OPEN");
    unlink("CLOSE");
    unlink("STOP");
    unlink("STATUS");
    exit(0);
}

void
fork_terminated(int s){
    exit(0);
}

void
alarmus(int s){
    alarm(1);
}

int
script_creator(const char *script_name, pid_t pid, int signal_id) {
    FILE *file = fopen(script_name, "w+");
    char *start_str = "#!/bin/bash\nkill";
    fprintf(file, "%s -%d", start_str, signal_id);
    fprintf(file, " %d\n", pid);
    fclose(file);
    return 0;
}

void
from_thermometer(int s){
    status = 1;
}

void
to_thermometer(int s){
    write(fd[1], &current_temp, sizeof(current_temp));
    kill(pid1, SIGTEMP1);

}

void
with_thermometer(int s){
    kill(pid1,SIGTEMP2);
    alarm(6);
}


void
from_modul(int s){
    if (s == SIGFREON1){
        if (status == 0) {
            printf("Freon is ON\n");
        }
        status = 1;
        repeat = 0;
    } else {
        if (status == 1){
            printf("Freon is OFF\n");
        }
        status = 0;
        repeat = 0;
    }
}

void
from_door1(int s){
    alarm(5);
    status = 1;
}

void
from_door2(int s){
    alarm(0);
    status = 0;
    alarm_status = 0;
}

void
start_shout(int s){
    alarm_status = 1;
}

void
from_user1(int s){
    if (status == 1){
        repeat = 1;
        return;
    }
    status = 1;
    repeat = 0;
    state = 0;
}

void
from_user2(int s){
    if (status == 0){
        repeat = 1;
        return;
    }
    repeat = 0;
    status = 0;
    state = 0;
}

void
alarmus_camera(int s){
    alarm(TIME_CAMERA);
    status = 1;
}


void
close_door(int s){
    repeat = 0;
    status = 0;
    state = 0;
}


int
main(void) {

    signal(SIGINT, current_status);
    signal(SIGTERM, main_terminated);
    int fd1[2];
    pipe(fd1);

    int fd_modul[2];
    pipe(fd_modul);

    if ((pid[0] = fork()) == 0) {               // modul of management
        signal(SIGTEMP1, from_thermometer);
        signal(SIGALRM, with_thermometer);
        signal(SIGTERM, fork_terminated);
        signal(SIGINT, modul_status);
        close(fd1[1]);
        read(fd1[0], &pid1, sizeof(pid1));      //pid of thermometer
        int pid_freon;
        read(fd1[0], &pid_freon, sizeof(pid_freon));
        close(fd_modul[1]);
        read(fd_modul[0], &current_temp, sizeof(current_temp));
        alarm(TIME_MODUL);
        printf("Modul is ready\n");
        while (1) {
            pause();
            if (status == 1) {
                read(fd_modul[0], &current_temp, sizeof(current_temp));
                if (current_temp >= 6){                     // check limits of temp
                    kill(pid_freon, SIGFREON1);
                }
                if (current_temp <= -1){
                    kill(pid_freon, SIGFREON2);
                }
                status = 0;
            }
        }
    }


    int fd2[2];
    pipe(fd2);

    int fd_thermometer[2];
    pipe(fd_thermometer);

    if ((pid[1] = fork()) == 0) {               //   thermometer
        signal(SIGTEMP2, to_thermometer);
        signal(SIGALRM, alarmus);
        signal(SIGTERM, fork_terminated);
        signal(SIGINT, thermometer_status);
        close(fd1[0]);
        close(fd1[1]);
        close(fd2[1]);
        int pid_modul;
        read(fd2[0], &pid_modul, sizeof(pid_modul));
        pid1 = pid_modul;
        close(fd_modul[0]);
        fd[1] = fd_modul[1];
        read(fd_thermometer[0], &current_temp, sizeof(current_temp));
        write(fd_modul[1], &current_temp, sizeof(current_temp));
        close(fd_thermometer[1]);
        printf("Thermometer is ready\n");
        alarm(1);
        while (1) {
            pause();
            read(fd_thermometer[0], &current_temp, sizeof(current_temp));       //read the last temp
        }

    }

    close(fd_modul[1]);
    close(fd_modul[0]);

    int fd_air[2];
    pipe(fd_air);

    if ((pid[2] = fork()) == 0){                //  freon
        signal(SIGFREON1, from_modul);
        signal(SIGFREON2, from_modul);
        signal(SIGTERM, fork_terminated);
        signal(SIGINT, freon_status);
        close(fd1[0]);
        close(fd1[1]);
        close(fd2[0]);
        close(fd2[1]);
        close(fd_thermometer[0]);
        close(fd_thermometer[1]);
        read(fd_air[0], &current_temp, sizeof(current_temp));
        write(fd_air[1], &current_temp, sizeof(current_temp));
        printf("Freon is OFF\n");
        while(1){
            pause();
            while(status) {
                read(fd_air[0], &current_temp, sizeof(current_temp));
                if (repeat == 0) {
                    current_temp = current_temp - FREON;   //frost the temp
                }
                repeat = 0;
                write(fd_air[1], &current_temp, sizeof(current_temp));
                sleep(TIME_FREON);
            }

        }
    }

    write(fd1[1], &pid[1], sizeof(pid[1]));
    write(fd1[1], &pid[2], sizeof(pid[2]));
    close(fd1[0]);
    close(fd1[1]);
    write(fd2[1], &pid[0], sizeof(pid[0]));
    close(fd2[0]);
    close(fd2[1]);




    if ((pid[3] = fork()) == 0){                //  air in camera
        signal(SIGALRM, alarmus_camera);
        signal(SIGTERM, fork_terminated);
        signal(SIGINT, air_status);
        close(fd_thermometer[0]);
        write(fd_thermometer[1], &current_temp, sizeof(current_temp));
        write(fd_air[1], &current_temp, sizeof(current_temp));
        int i = 0;
        alarm(TIME_CAMERA);
        while (1) {
            pause();
            if (status == 1) {
                i++;
                read(fd_air[0], &current_temp, sizeof(current_temp));
                if (i == 3) {
                    i = 0;
                    current_temp = current_temp + CAMERA;
                }
                printf("TEMP = %d\n", current_temp);
                write(fd_air[1], &current_temp, sizeof(current_temp));
                write(fd_thermometer[1], &current_temp, sizeof(current_temp));
            }
        }


    }

    close(fd_thermometer[0]);
    close(fd_thermometer[1]);


    int fd3[2];
    pipe(fd3);

    if ((pid[4] = fork()) == 0){                //  door
        signal(SIGOPENDOOR, from_user1);
        signal(SIGCLOSEDOOR, from_user2);
        signal(SIGCLOSE, close_door);
        signal(SIGTERM, fork_terminated);
        signal(SIGINT, door_status);
        close(fd3[1]);
        int pid_signalization;
        read(fd3[0], &pid_signalization, sizeof(pid_signalization));
        read(fd_air[0], &current_temp, sizeof(current_temp));
        write(fd_air[1], &current_temp, sizeof(current_temp));
        printf("Door is closed\n");
        while(1) {
            pause();
            if (state == 0) {
                if (status == 1) {
                    printf("Door is open!\n");
                    kill(pid_signalization,SIGOPENDOOR);
                    while(status) {
                        read(fd_air[0], &current_temp, sizeof(current_temp));
                        if (repeat == 0) {
                            current_temp = current_temp + DOOR;
                        }
                        repeat = 0;
                        write(fd_air[1], &current_temp, sizeof(current_temp));
                        sleep(TIME_DOOR);
                    }

                }
                if (status == 0) {
                    if (repeat == 0) {
                        printf("Door is closed\n");
                    }
                    kill(pid_signalization,SIGCLOSEDOOR);
                }
            }
        }
    }

    close(fd_air[0]);
    close(fd_air[1]);

    if ((pid[5] = fork()) == 0){                //  signalization
        signal(SIGOPENDOOR, from_door1);
        signal(SIGCLOSEDOOR, from_door2);
        signal(SIGALRM, start_shout);
        signal(SIGTERM, fork_terminated);
        signal(SIGINT, signalization_status);
        printf("Signalization is OFF\n");
        while (1) {
            pause();
            if (alarm_status == 1) {
                int i = 0;
                printf("Signalization is ON\n");
                while (status) {
                    if (i != PEEEEP_AMOUNT) {
                        printf("PEEEEEEP\n");
                    } else {
                        printf("PEEEEEEP...\n");
                        kill(pid[4],SIGCLOSE);
                        break;
                    }
                    sleep(TIME_PEEEEP);
                    i++;
                }
                printf("Signalization is OFF\n");
                alarm_status = 0;
                status = 0;
            }
        }


    }

    write(fd3[1], &pid[5], sizeof(pid[5]));
    close(fd3[0]);
    close(fd3[1]);
    script_creator("OPEN", pid[4], SIGOPENDOOR);
    script_creator("CLOSE", pid[4], SIGCLOSEDOOR);
    script_creator("STOP", getpid(),SIGTERM);
    script_creator("STATUS", getpid(), SIGINT);
    while(1){
        pause();
    }
}
