#include <stdio.h>
#include <unistd.h>
#include <_stdlib.h>

// state gas pump can be
typedef enum {
    STATE_IDLE,
    STATE_AUTHORIZED,
    STATE_NOT_AUTHORIZED,
    STATE_EXIT
} gas_pump_t;

typedef enum {
    EVENT_AUTHORIZED,
    EVENT_NOT_AUTHORIZED,
    EVENT_EXIT
} event_t;

// print states
void state_idle() {
    printf("idle...\n");
}

void state_authorized() {
    printf("authorized...\n");
}

void state_pumping() {
    printf("pumping...\n");
}

void state_not_authorized() {
    printf("not authorized...\n");
}

void state_complete() {
    printf("pump complete...\n");
}

void state_exit() {
    printf("exit...\n");
}

int gallons, cash;
gas_pump_t current_state = STATE_IDLE;
event_t current_event;

event_t get_event() {
    printf("Welcome to the Gas Pump\n");

    printf("\nPlease enter the number of gallons: ");
    scanf("%d", &gallons);

    printf("\nPlease enter the number of cash on hand: ");
    scanf("%d", &cash);

    if(gallons == 0) {
        state_exit();
        current_state = STATE_EXIT;
        return EVENT_NOT_AUTHORIZED;
    }

    if((gallons * 2) <= cash) {
        state_authorized();
        current_state = STATE_AUTHORIZED;
        return EVENT_AUTHORIZED;
    }
    else {
        state_not_authorized();
        current_state = STATE_NOT_AUTHORIZED;
        return EVENT_NOT_AUTHORIZED;
    }
}

void print_reciept() {
    printf("*****************************************************\n\n");
    printf("gallons: %d\n", gallons);
    printf("Total %0.2f\n\n", (float)(gallons * 2.0));
    printf("Have a bad rest of your day!\n\n");
    printf("\n\n\nBTW I sent your money to the somali pirates\n");
    printf("*****************************************************\n\n");
}

void reset_state() {
    gallons = 0;
    cash = 0;
    current_state = STATE_IDLE;
    current_event = EVENT_NOT_AUTHORIZED;
}

int main(void) {
    while(1) {
        current_event = get_event();

        if((current_event == EVENT_NOT_AUTHORIZED) && (current_state == STATE_EXIT)) {
            return 0;
        }
        else if(current_event == EVENT_NOT_AUTHORIZED) {
            continue;
        }

        state_pumping();
        sleep(gallons * 2);
        state_complete();
        print_reciept();
    }
}
