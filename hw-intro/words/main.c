/*

  Word Count using dedicated lists

*/

/*
Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <assert.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "word_count.h"

/* Global data structure tracking the words encountered */
WordCount *word_counts = NULL;

/* The maximum length of each word in a file */
#define MAX_WORD_LEN 64

/*
 * 3.1.1 Total Word Count
 *
 * Returns the total amount of words found in infile.
 * Useful functions: fgetc(), isalpha().
 */
int num_words(FILE* infile) {
  if (infile == NULL) {
    return 0;
  }
  int num_words = 0;

  // what is the definition of a word? when do we stop counting a word?
  // word is a series of continguous alphabetical chars that match the limit
  // AND until a non-alphabetical char is found


  // how to identify a word:
  // increment a counter for each alphabetical char you see
  // the minute you see a non-alphabetical char, you check your counter and if > 1, increment
  // num_words by 1 and reset the local counter
  int local_word_counter = 0;
  int c;
  while ((c = fgetc(infile)) != EOF) {
    // char is alphabetical
    // printf("Current char in infile: %c\n", c);
    if (isalpha(c)) {
      local_word_counter++;
    } else {
      // char is non-alphabetical
      if (local_word_counter > 1) {
        num_words++;
        local_word_counter = 0;
      }
    }
  }
  // edge case where a word is counted before EOF so that word is not counted
  if (local_word_counter > 1) {
    num_words++;
  }
  return num_words;
}

/*
 * 3.1.2 Word Frequency Count
 *
 * Given infile, extracts and adds each word in the FILE to `wclist`.
 * Useful functions: fgetc(), isalpha(), tolower(), add_word().
 * 
 * As mentioned in the spec, your code should not panic or
 * segfault on errors. Thus, this function should return
 * 1 in the event of any errors (e.g. wclist or infile is NULL)
 * and 0 otherwise.
 */
int count_words(WordCount **wclist, FILE *infile) {
  // need a way to gather all the found characters together so far
  char current_word[MAX_WORD_LEN];
  int local_word_counter = 0;
  int c;
  while ((c = fgetc(infile)) != EOF) {
    // char is alphabetical
    // printf("Current char in infile: %c\n", c);
    if (isalpha(c)) {
      if (local_word_counter == MAX_WORD_LEN) {
        return 1;
      }
      current_word[local_word_counter] = (char) tolower(c);
      local_word_counter++;
    } else {
      // char is non-alphabetical
      if (local_word_counter > 1) {
        // once you have found a complete word, process it
        current_word[local_word_counter] = '\0';
        // printf("current word parsed: %s\n", current_word);
        int result = add_word(wclist, current_word);
        if (result) {
          // printf("Goes into add_word failed case");
          return 1;
        } else {
          // printf("Goes into add_word succeeded case");
          local_word_counter = 0;
        }
        
      }
    }
  }
  // edge case where a word is counted before EOF so that word is not process
  if (local_word_counter > 1) {
    // process this last word
    current_word[local_word_counter] = '\0';
    int result = add_word(wclist, current_word);
    if (result) {
      return 1;
    }
  }
  return 0;
}

/*
 * Comparator to sort list by frequency.
 * Useful function: strcmp().
 */
static bool wordcount_less(const WordCount *wc1, const WordCount *wc2) {
  if (wc1 == NULL || wc2 == NULL) {
    exit(1);
  }
  // first check the count - if same proceed with comparision of words
  int wc1Count = wc1->count;
  int wc2Count = wc2->count;
  if (wc1Count < wc2Count) {
    return true;
  } else if (wc1Count > wc2Count) {
    return false;
  } else {
    // compare word strings here
    int result =  strcmp(wc1->word, wc2->word);
    if (result < 0) {
      return true;
    } else if (result > 0) {
      return false;
    } else {
      // non-existent state of two nodes having same words - exit program in this case
      exit(1);
    }
  }
}

// In trying times, displays a helpful message.
static int display_help(void) {
	printf("Flags:\n"
	    "--count (-c): Count the total amount of words in the file, or STDIN if a file is not specified. This is default behavior if no flag is specified.\n"
	    "--frequency (-f): Count the frequency of each word in the file, or STDIN if a file is not specified.\n"
	    "--help (-h): Displays this help message.\n");
	return 0;
}

/*
 * Handle command line flags and arguments.
 */
int main (int argc, char *argv[]) {

  // Count Mode (default): outputs the total amount of words counted
  bool count_mode = true;
  int total_words = 0;

  // Freq Mode: outputs the frequency of each word
  bool freq_mode = false;

  FILE *infile = NULL;

  // Variables for command line argument parsing
  int i;
  static struct option long_options[] =
  {
      {"count", no_argument, 0, 'c'},
      {"frequency", no_argument, 0, 'f'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}
  };

  // Sets flags
  while ((i = getopt_long(argc, argv, "cfh", long_options, NULL)) != -1) {
      switch (i) {
          case 'c':
              count_mode = true;
              freq_mode = false;
              break;
          case 'f':
              count_mode = false;
              freq_mode = true;
              break;
          case 'h':
              return display_help();
      }
  }

  if (!count_mode && !freq_mode) {
    printf("Please specify a mode.\n");
    return display_help();
  }

  /* Create the empty data structure */
  init_words(&word_counts);

  if ((argc - optind) < 1) {
    // No input file specified, instead, read from STDIN instead.
    infile = stdin;
    total_words =  num_words(infile);
    // printf("total_words is %d\n", total_words);

  } else {
    // Test out single file inputs for the frequency program
    // [FREQUENCY PROGRAM]
    char* file = argv[optind];
    // printf("%c\n", *(file+1));
    infile = fopen(file, "r");
    if (infile == NULL) {
      exit(-1);
    }
    count_words(&word_counts, infile);
    fclose(infile);




    // At least one file specified. Useful functions: fopen(), fclose().
    // The first file can be found at argv[optind]. The last file can be
    // found at argv[argc-1].

   // we can have multiple files specified
   // iterate from optind till argc-1 (inclusive) to get all files to read from
   // each file index can use fopen to read disk from and pull into memory
  //  for (int i = optind; i < argc; i++) {
  //   char* currentFileToReadFrom = argv[i];
  //   infile = fopen(currentFileToReadFrom, "r");
  //   if (infile == NULL) {
  //     exit(-1);
  //   }
  //   int numWordsInReadFile = num_words(infile);
  //   total_words += numWordsInReadFile;
  //   fclose(infile);
  //  }
  }

  if (count_mode) {
    printf("The total number of words is: %i\n", total_words);
  } else {
    wordcount_sort(&word_counts, wordcount_less);
    printf("The frequencies of each word are: \n");
    fprint_words(word_counts, stdout);
}
  return 0;
}
