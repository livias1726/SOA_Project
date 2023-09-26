#include "user.h"

int main(int argc, char *argv[]){

    if (check_input(argc, argv)) return -1;

    while(1) {

        printf("Choose an operation:\n"
               "\t[1] Put data\n"
               "\t[2] Get data\n"
               "\t[3] Invalidate data\n"
               "\t[other] Exit\n");

        switch(getint()){
            case 1:
                test_put_data();
                break;
            case 2:
                test_get_data();
                break;
            case 3:
                test_invalidate_data();
                break;
            default:
                return 0;
        }
    }
}