#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define swap(T, a, b) do { T tmp = a; a = b; b = tmp; } while(0)

static void usage(const char *progname)
{
  fprintf(stderr, "Usage: %s [-b|--base BASE] [FILES...]\n", progname);
  exit(EXIT_FAILURE);
}

static char *base;

static char **files;
static int file_count;

static void parse_args(int argc, char *argv[])
{
  files = argv;
  for(int i=1; i<argc; ++i)
    if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--base") == 0)
    {
      i += 1;

      if(i==argc)
      {
        fprintf(stderr, "ERROR: -b|--base must be used with an argument\n");
        usage(argv[0]);
      }

      if(base)
      {
        fprintf(stderr, "ERROR: -b|--base can only be specified once\n");
        usage(argv[0]);
      }

      char *path = realpath(argv[i], NULL);
      if(!path)
      {
        fprintf(stderr, "Error: Failed to resolve %s: %s\n", argv[i], strerror(errno));
        exit(EXIT_FAILURE);
      }
      base = strcmp(path, "/") != 0 ? path : "";
    }
    else
    {
      char *path = realpath(argv[i], NULL);
      if(!path)
      {
        fprintf(stderr, "Error: Failed to resolve %s: %s\n", argv[i], strerror(errno));
        continue;
      }
      files[file_count++] = path;
    }
}

static const char *remove_prefix(const char *str, const char *prefix)
{
  size_t n = strlen(prefix);
  if(strncmp(str, prefix, n) == 0)
    return str + n;
  else
    return NULL;
}

static void process_file(const char *base, char *file)
{
  static char *line1 = NULL, *line2 = NULL;
  static size_t n1, n2 = 0;

  char *header_guard = NULL;

  FILE *f = NULL;

  FILE *tmp = NULL;
  char *tmp_buf = NULL;
  size_t tmp_size = 0;

  //////////////////
  // Header Guard //
  //////////////////
  const char *s = file;
  if(!(s = remove_prefix(s, base)) ||
     !(s = remove_prefix(s, "/")))
  {
    fprintf(stderr, "Error: %s: File path does not begin with base (%s/)\n", file, base);
    goto exit;
  }

  header_guard = strdup(s);
  for(char *p = header_guard; *p; ++p)
  {
    *p = toupper(*p);
    if(*p == '/' || *p == '.')
      *p = '_';
  }

  ///////////////
  // Open File //
  ///////////////
  if(!(f = fopen(file, "rb")))
  {
    fprintf(stderr, "Error: %s: Failed to open file for reading: %s\n", file, strerror(errno));
    goto exit;
  }

  //////////////
  // Open Tmp //
  //////////////
  if(!(tmp = open_memstream(&tmp_buf, &tmp_size)))
  {
    fprintf(stderr, "Error: %s: Failed to create temporary buffer: %s\n", file, strerror(errno));
    goto exit;
  }

  /////////////
  // #ifndef //
  /////////////
  if(getline(&line1, &n1, f) == -1)
  {
    fprintf(stderr, "Error: %s: Failed to read from file: %s\n", file, feof(f) ? "End of file" : strerror(errno));
    goto exit;
  }

  if(strncmp(line1, "#ifndef ", 7) != 0)
  {
    fprintf(stderr, "Error: %s: First line does not begin with #ifndef\n", file);
    goto exit;
  }

  fprintf(tmp, "#ifndef %s\n", header_guard);

  /////////////
  // #define //
  /////////////
  if(getline(&line1, &n1, f) == -1)
  {
    fprintf(stderr, "Error: %s: Failed to read from file: %s\n", file, feof(f) ? "End of file" : strerror(errno));
    goto exit;
  }

  if(strncmp(line1, "#define ", 7) != 0)
  {
    fprintf(stderr, "Error: %s: Second line does not begin with #define\n", file);
    goto exit;
  }

  fprintf(tmp, "#define %s\n", header_guard);

  ////////////
  // #endif //
  ////////////
  if(getline(&line1, &n1, f) == -1)
  {
    fprintf(stderr, "Error: %s: Failed to read from file: %s\n", file, feof(f) ? "End of file" : strerror(errno));
    goto exit;
  }

  while(getline(&line2, &n2, f) != -1)
  {
    fprintf(tmp, "%s", line1);
    swap(char *, line1, line2);
    swap(size_t, n1, n2);
  }

  if(!feof(f))
  {
    fprintf(stderr, "Error: %s: Failed to read from file: %s\n", file, strerror(errno));
    goto exit;
  }

  if(strncmp(line1, "#endif ", 7) != 0)
  {
    fprintf(stderr, "Error: %s: Last line does not begin with #endif\n", file);
    goto exit;
  }

  fprintf(tmp, "#endif // %s\n", header_guard);

  ///////////
  // Flush //
  ///////////
  if(fflush(tmp) != 0)
  {
    fprintf(stderr, "Error: %s: Failed to flush temporary buffer: %s\n", file, strerror(errno));
    goto exit;
  }

  /////////////////
  // Reopen File //
  /////////////////
  if(fclose(f) != 0)
  {
    fprintf(stderr, "Error: %s: Failed to close file: %s\n", file, strerror(errno));
    f = NULL;
    goto exit;
  }
  else if(!(f = fopen(file, "wb")))
  {
    fprintf(stderr, "Error: %s: Failed to open file for wriring: %s\n", file, strerror(errno));
    goto exit;
  }

  ////////////
  // Commit //
  ////////////
  if(fwrite(tmp_buf, tmp_size, 1, f) != 1)
  {
    fprintf(stderr, "Error: %s: Failed to write to file: %s\n", file, strerror(errno));
    goto exit;
  }

  //////////
  // Exit //
  //////////
exit:
  if(f && fclose(f) != 0)
  {
    fprintf(stderr, "Error: %s: Failed to close file\n", file);
    goto exit;
  }

  if(tmp && fclose(tmp) != 0)
  {
    fprintf(stderr, "Error: %s: Failed to close temporary output buffer\n", file);
    goto exit;
  }

  free(tmp_buf);
  free(header_guard);
}

int main(int argc, char *argv[])
{
  parse_args(argc, argv);
  for(int i=0; i<file_count; ++i)
    process_file(base, files[i]);
}

