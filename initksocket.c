/*
== == == == == == == == == == == == == == == == == ==
Mini Project 1 Submission
Group Details :
Member 1 Name : Dake Yuvan Gates
Member 1 Roll number : 23CS10015
Member 2 Name : Marala Sai Pragnaan
Member 2 Roll number : 23CS10043
== == == == == == == == == == == == == == == == == ==
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct swnd
{
    int seq_num[256];
    int window_size;
} typedef swnd;

struct rwnd
{
    int seq_num[256];
    int window_size;
} typedef rwnd;

int main()
{
    pthread_t R, S;
    
}
