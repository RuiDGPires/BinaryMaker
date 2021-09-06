/* binarymaker.c
 *******************
 *MIT License
 *
 *Copyright (c) 2021 Rui Pires
 *
 *Permission is hereby granted, free of charge, to any person obtaining a copy
 *of this software and associated documentation files (the "Software"), to deal
 *in the Software without restriction, including without limitation the rights
 *to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *copies of the Software, and to permit persons to whom the Software is
 *furnished to do so, subject to the following conditions:
 *
 *The above copyright notice and this permission notice shall be included in all
 *copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *SOFTWARE.
 */

/*
 * Using 3 threads, the process is done as it shows:
 *
 *  [reading_buffer]  ---> << converting_thread >> ---> [writing_buffer]
 *         /\                                                 ||
 *         ||             «converts 2 chars to u8»            ||
 *    Writes chars to                                         ||
 *         ||                                                 ||
 *         ||                                                 \/ 
 * << reading_thread >>                              << writing_thread >>
 *         ||                                                 ||
 *         ||                                                 ||
 *     Reads From                                         Writes to
 *         ||                                                 ||
 *         \/                                                 \/
 *    (input file)                                      (output file)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef WIN32 // UNIX 
#include "winpthreads.h"
#else // WINDOWS
#include <pthread.h>
#endif

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

#ifndef bool
typedef uint8_t bool;
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define DISPLAY_HELP()\
	{\
	printf("       Text To Binary v0.1.0       \n");\
	printf("___________________________________\n\n");\
	printf("Usage:\n");\
	printf("\ttxttobin <filein> <fileout>\n");\
	printf("---\n");\
	printf("\tfilein: Text file with hexadecimal numbers separated by whitespace or paragraphs\n");\
	printf("\tfileout: Output binary\n");\
	exit(0);\
	}

#ifdef DEBUG
#define PRINT_FUNCTION_NAME() fprintf(stderr, " at %s:\n\t", __func__);
#else
#define PRINT_FUNCTION_NAME() fprintf(stderr, ":\n\t");
#endif

#define THROW_ERROR(...) \
	{\
		fflush(stdout);\
		fprintf(stderr, "\033[0;31m"); \
		fprintf(stderr, "Error"); \
		PRINT_FUNCTION_NAME() \
		fprintf(stderr, "\""); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\"\n\033[0m"); \
		exit(-1);\
	}

#define REPEAT(times)\
	for (int _i = 0; _i < times; _i++)

#ifdef DEBUG
#define DEBUG_PRINT(...)\
	printf(__VA_ARGS__);
#else
#define DEBUG_PRINT(...)
#endif

void *mallocWithError(size_t size){
  void *p = malloc(size);
  if (p == NULL) THROW_ERROR("Unable to allocate memory");
  return p;
}

#define BUFFER_SIZE 512 
#define DUMP_SIZE BUFFER_SIZE / 2 

pthread_mutex_t reading_mutex, writing_mutex;
pthread_cond_t reading_can_produce, reading_can_consume, writing_can_produce, writing_can_consume;
u32 reading_producer_index, reading_consumer_index, writing_producer_index, writing_consumer_index;

char reading_buffer[BUFFER_SIZE];
u8 writing_buffer[BUFFER_SIZE]; 
bool reading_buffer_free, writing_buffer_free;


// THREAD CONTROL //////////
void mutexLock(pthread_mutex_t *mutex) {
	if (pthread_mutex_lock(mutex) != 0) THROW_ERROR("Error locking mutex");
}

void mutexUnlock(pthread_mutex_t *mutex) {
	if (pthread_mutex_unlock(mutex) != 0) THROW_ERROR("Error unlocking mutex");
}

void waitCondition(pthread_cond_t *cond, pthread_mutex_t *mutex){
	if (pthread_cond_wait(cond, mutex) != 0) THROW_ERROR("Unable to wait for condition");
}

void signalCondition(pthread_cond_t *cond){
	if (pthread_cond_signal(cond) != 0) THROW_ERROR("Unable to signal condition");
}
///////////////////////////


u32 getDistanceInBuffer(u32 a, u32 b){
	return a <= b? b - a: BUFFER_SIZE - a + b;  
}

void *readFile(void *arg){
	char *filename = (char *) arg;
	FILE *file = fopen(filename, "r");
	if (file == NULL) THROW_ERROR("Unable to open file: %s", filename);

	int c;
	char tmp[DUMP_SIZE];

	do{
		c =	fread(tmp, sizeof(char), DUMP_SIZE, file);

		mutexLock(&reading_mutex);
		// Wait until able to write
		while (getDistanceInBuffer(reading_producer_index, reading_consumer_index) < c + 1)
			waitCondition(&reading_can_produce, &reading_mutex);

		for (int i = 0; i < c; i++){
			reading_buffer[(reading_producer_index + i) % BUFFER_SIZE] = tmp[i];
		}

		reading_producer_index = (reading_producer_index + c) % BUFFER_SIZE;

		signalCondition(&reading_can_consume);
		mutexUnlock(&reading_mutex);	
	}while(c != 0);

	// Check if any error occured
	if (ferror(file)) THROW_ERROR("An error occured while reading file");

	fclose(file);	
	reading_buffer_free = TRUE;
	signalCondition(&reading_can_consume);
	pthread_exit(NULL);
	return NULL;
}

u8 convertCharsToU8(const char chars[]){
	u8 result = 0;
	for (int i = 0; i < 2 ; i ++){
		result <<= 4;
		if (chars[i] >= '0' && chars[i] <= '9') // IF CHAR IS NUMBER
			result |= (u8) ((int)chars[i] - (int) '0');
		else if (chars[i] >= 'A' && chars[i] <= 'F') // IF IS UPPER LETTER
			result |= (u8) ((int)chars[i] - (int) 'A' + 10);
		else if (chars[i] >= 'a' && chars[i] <= 'f') // IF IS UPPER LETTER
			result |= (u8) ((int)chars[i] - (int) 'a' + 10);
		else THROW_ERROR("UNKOWN CHARACTER: %c", chars[i]);	
	}
	return result;
}

char vals[2];

void *convertFile(void *arg){
	(void) arg;
	u8 count = 0;

	while(!reading_buffer_free || getDistanceInBuffer(reading_consumer_index, reading_producer_index) != 1){
		// READ CHARS FROM READING BUFFER
		mutexLock(&reading_mutex);
		u32 dist = getDistanceInBuffer(reading_consumer_index, reading_producer_index);

		while(!reading_buffer_free && dist <= 1){
			waitCondition(&reading_can_consume, &reading_mutex);
			dist = getDistanceInBuffer(reading_consumer_index, reading_producer_index);
		}
	
		if (reading_buffer_free && dist == 1) continue;
		reading_consumer_index = (reading_consumer_index + 1) % BUFFER_SIZE;

		char c = reading_buffer[reading_consumer_index];
		if (c != ' ' && c != 0 && c != '\0' && c != '\r' && c != '\n' && c != '\t')
			vals[count++] = c;
		
		signalCondition(&reading_can_produce);
		mutexUnlock(&reading_mutex);

		if (count == 2){
			u8 val = convertCharsToU8(vals);
			DEBUG_PRINT("CONVERTED TO: %x\n", val);
			// WRITE CONVERTED CHARS TO WRITING BUFFER
			mutexLock(&writing_mutex);
				
			while(getDistanceInBuffer(writing_producer_index, writing_consumer_index) == 1)
				waitCondition(&writing_can_produce, &writing_mutex);

			writing_buffer[writing_producer_index] = val;
			writing_producer_index = (writing_producer_index + 1) % BUFFER_SIZE;

			signalCondition(&writing_can_consume);
			mutexUnlock(&writing_mutex);
			count = 0;
		}	
	}
	writing_buffer_free = TRUE;
	signalCondition(&writing_can_consume);
	pthread_exit(NULL);
	return NULL;
}

void *writeFile(void *arg){
	char *filename = (char *) arg;
	FILE *file = fopen(filename, "wb");
	if (file == NULL) THROW_ERROR("Unable to open file: %s", filename);

	u8 tmp[BUFFER_SIZE];

	while(!writing_buffer_free || getDistanceInBuffer(writing_consumer_index, writing_producer_index) != 1){	
		mutexLock(&writing_mutex);

		u32 dist = getDistanceInBuffer(writing_consumer_index, writing_producer_index);

		while (!writing_buffer_free && dist < DUMP_SIZE){
			waitCondition(&writing_can_consume, &writing_mutex);
			dist = getDistanceInBuffer(writing_consumer_index, writing_producer_index);
		}
		

		for (int i = 0; i < dist - 1; i++){
			tmp[i] = writing_buffer[(writing_consumer_index + i + 1) % BUFFER_SIZE];
			DEBUG_PRINT("Writing: %x\n", tmp[i]);
		}

		writing_consumer_index = (writing_consumer_index + dist - 1)%BUFFER_SIZE;
		signalCondition(&writing_can_produce);
		mutexUnlock(&writing_mutex);	

	

		fwrite(tmp, sizeof(u8), dist - 1, file);
	}
	
	fclose(file);	
	pthread_exit(NULL);
	return NULL;
}

pthread_t *createReadingThread(char file[]){
	pthread_t *thread = (pthread_t *) mallocWithError(sizeof(pthread_t));
	
	if (pthread_create(thread, NULL, readFile, (void *) file))
			THROW_ERROR("Couldnt create thread");
	return thread;
}

pthread_t *createConvertingThread(){
	pthread_t *thread = (pthread_t *) mallocWithError(sizeof(pthread_t));
	
	if (pthread_create(thread, NULL, convertFile, NULL))
			THROW_ERROR("Couldnt create thread");
	return thread;
}

pthread_t *createWritingThread(char file[]){
	pthread_t *thread = (pthread_t *) mallocWithError(sizeof(pthread_t));
	
	if (pthread_create(thread, NULL, writeFile, (void *) file))
			THROW_ERROR("Couldnt create thread");
	return thread;
}


int main(int argc, char *argv[]){
	if (argc == 2)
		if (argv[1][0] == '-' && argv[1][1] == 'h') // PRINT HELP
			DISPLAY_HELP();
	

	if (argc != 3) THROW_ERROR("Invalid number of command line arguments");
	char *file_in = argv[1];
	char *file_out = argv[2];	
	
	pthread_mutex_init(&reading_mutex, NULL);	
	pthread_mutex_init(&writing_mutex, NULL);	

	pthread_cond_init(&reading_can_consume, NULL);
	pthread_cond_init(&reading_can_produce, NULL);
	pthread_cond_init(&writing_can_consume, NULL);
	pthread_cond_init(&writing_can_produce, NULL);

	reading_consumer_index = BUFFER_SIZE -1;
	writing_consumer_index = BUFFER_SIZE -1;
	reading_producer_index = 0;
	writing_producer_index = 0;

	reading_buffer_free = FALSE;
	writing_buffer_free = FALSE;

	// CREATE WORKING THREADS 
	pthread_t *readingThread = createReadingThread((void *) file_in);
	pthread_t *convertingThread = createConvertingThread();
	pthread_t *writingThread = createWritingThread((void *) file_out);

	// WAIT UNTIL PROCESSING IS FINISHED
	pthread_join(*readingThread, NULL);
	pthread_join(*convertingThread, NULL);
	pthread_join(*writingThread, NULL);

	free(readingThread);
	free(convertingThread);
	free(writingThread);
	
	pthread_mutex_destroy(&reading_mutex);
	pthread_mutex_destroy(&writing_mutex);

	return 0;
}
