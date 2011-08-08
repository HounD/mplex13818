/*
 * tiny debugging tool.
 * when fed with a transport stream to stdin, produces one char
 * per packet to stdout, and a few informative messages to stderr.
 * Upper case letters denote a continuity counter mismatch.
 *
 * Author: Jacob Schroeder (jacob@convergence.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MAXPID 0x1fff

static struct option long_options[] = {
	{"help"   , 0, NULL, 'h'},
	{"summary", 0, NULL, 's'},
	{"analyse", 0, NULL, 'a'},
	{"pcr"    , 0, NULL, 'P'},
	{"start",   0, NULL, 'S'},
	{NULL, 0, NULL, 0}
};

static const char *program = NULL;

static int         show_analysis = 0;
static int         show_summary = 0;
static int         show_packets = 1;
static int         show_pids    = 1;

static int         show_cc_jumps = 1;
static int         show_pcr      = 0;
static int         show_start    = 0;

static int         pid_cnt = 0;
static int         packet_cnt = 0;
static int         broken_sync_cnt = 0;
static int         broken_alen_cnt = 0;
static int         unusual_cc_cnt  = 0;

static void usage ()
{
	printf("Usage: %s [OPTION] < <TS file>\n"
                        " -h, --help                    show usage\n"
                        " -s, --summary                 show summary\n"
                        " -a, --analyse                 analyse stream statistics\n"
                        " -P, --pcr                     indicate PCR\n"
                        " -S, --start                   indicate unit start\n"
			, program);
}


int main (int argc, char **argv)
{

  int    pids[MAXPID+1];
  int    ccs[MAXPID+1];
  int    pid_count = 0;
  int    i;
  int    count = 0;

  char **next_arg = argv+1;
  int    option_index = 0;

  if ((program = strrchr(argv[0], '/')) != NULL) {
      program++;
  } else {
      program = argv[0];
  }

  while (1) {
      int opt;
      // char *endptr;

      opt = getopt_long (argc, argv, "+hsPSa", long_options, &option_index);
      if (opt == -1) break;

      switch (opt) {
        case 'a':
          show_analysis = 1;
          show_packets = 0;
          break;

        case 's':
          show_summary = 1;
          show_packets = 0;
          break;

        case 'S':
          show_cc_jumps = 0;
          show_start    = 1;
          show_pcr      = 0;
          break;

        case 'P':
          show_cc_jumps = 0;
          show_start    = 0;
          show_pcr      = 1;
          break;

        case 'h':
          usage ();
          exit (0);

        default:
          usage ();
          exit (1);
      }
  }

  next_arg = argv + optind;

  for (i = 0; i <= MAXPID; i++)
    {
      pids[i] = -1;
      ccs[i] = -1;
    }

  while (1)
    {
      unsigned char buffer[188];
      int           bytes_read;

      int n;


      bytes_read = 0;

      do
        {
          n = read (0, buffer + bytes_read, sizeof (buffer) - bytes_read);

          if (n < 0)
            {
              if (errno == EINTR)
                continue;

              perror ("read");
              exit (1);
            }

          bytes_read += n;
        }
      while (bytes_read < sizeof (buffer) && n > 0);

      if (n == 0)
        {
          int exit_value = 0;

          if (bytes_read && bytes_read < sizeof (buffer))
            {
              fprintf (stderr, "incomplete ts packet read, size = %d\n", bytes_read);
            }

          if (show_summary)
            {
              printf ("packets:                        %-6d\n", packet_cnt);
              printf ("pids:                           %-6d\n", pid_cnt);
              printf ("sync errors:                    %-6d\n",
                      broken_sync_cnt);
              printf ("adaptation field length errors: %-6d\n",
                      broken_alen_cnt);
              printf ("unusual cc increments:          %-6d\n",
                      unusual_cc_cnt);
              printf ("\n");
            }

          if (show_analysis)
            {
              int quality = 1;

              if (pid_cnt > packet_cnt / 10)
                {
                  printf ("The number of different PIDs is higher than 10%% of the packet count.\n");
                  quality = -1;
                }
              else if (pid_cnt > packet_cnt / 100)
                {
                  printf ("The number of different PIDs is higher than 1%% of the packet count.\n");
                  if (quality > 0)
                    quality = 0;
                }

              if (pid_cnt > 2048)
                {
                  printf ("The number of different PIDs is higher than 2048.\n");
                  quality = -1;
                }

              if (broken_sync_cnt > 0)
                {
                  if (broken_sync_cnt < packet_cnt / 100)
                    {
                      printf ("There are packets with a broken sync byte but it's less than 1%%.\n");
                      if (quality > 0)
                        quality = 0;
                    }
                  else if (broken_sync_cnt > packet_cnt / 10)
                    {
                      printf ("More than 10%% of the packets have a broken sync byte.\n");
                      quality = -2;
                    }
                  else
                    {
                      printf ("More than 1%% of the packets have a broken sync byte.\n");
                      if (quality > -1)
                        quality = -1;
                    }
                }

              if (broken_alen_cnt > 0)
                {
                  if (broken_alen_cnt == 0)
                    {
                      printf ("All packets with a correct sync seem to have a consistent TS header.\n");
                    }
                  else if (broken_alen_cnt < packet_cnt / 100)
                    {
                      printf ("There are packets with an inconsistent TS header but it's less than 1%%.\n");
                      if (quality > 0)
                        quality = 0;
                    }
                  else if (broken_alen_cnt > packet_cnt / 10)
                    {
                      printf ("More than 10%% of the packets have an inconsistent TS header.\n");
                      quality = -2;
                    }
                  else
                    {
                      printf ("More than 1%% of the packets have an inconsistent TS header.\n");
                      if (quality > -1)
                        quality = -1;
                    }
                }

              if (quality == 1)
                {
                  printf ("This transport stream seems to be ok.\n");
                }
              else if (quality == 0)
                {
                  printf ("This transport stream has some flaws, but it might be ok.\n");
                }
              else if (quality == -1)
                {
                  printf ("It is very likely, that this transport stream is broken.\n");
                  exit_value = 2;
                }
              else
                {
                  printf ("This transport stream is broken.\n");
                  exit_value = 3;
                }
            }

          exit (exit_value);
        }

      packet_cnt++;

      // printf ("%02x%02x%02x%02x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
      {
        int   sync;
        int   pid;
        int   cc;
        int   ccok;

        int   error_ind;
        int   unit_start;
        int   priority;
        int   scrambling;
        int   adaptation;

        int   alen = 0;
        int   real_alen = 0;

        int   disc = 0;
        int   random = 0;
        int   espri = 0;
        int   pcr = 0;
        int   opcr = 0;
        int   splpt = 0;
        int   priv = 0;
        int   afext = 0;

        int   priv_len = 0;
        int   afext_len = 0;

        char  outbuf[1];

        int   pidtype;
        int   togglec;

        sync = buffer[0];
        error_ind  = (buffer[1] & 0x80) >> 7;
        unit_start = (buffer[1] & 0x40) >> 6;
        priority   = (buffer[1] & 0x20) >> 5;
        pid  = ((buffer[1] & 0x1f) << 8) | buffer[2];
        scrambling  = (buffer[3] & 0x60) >> 6;
        adaptation  = (buffer[3] & 0x30) >> 4;
        cc   = buffer[3] & 0x0f;

        if (adaptation & 0x02)
          {
            alen = buffer[4];
            if (alen > 0)
              {
                disc   = (buffer[5] & 0x80) >> 7;
                random = (buffer[5] & 0x40) >> 6;
                espri  = (buffer[5] & 0x20) >> 5;
                pcr    = (buffer[5] & 0x10) >> 4;
                opcr   = (buffer[5] & 0x08) >> 3;
                splpt  = (buffer[5] & 0x04) >> 2;
                priv   = (buffer[5] & 0x02) >> 1;
                afext  = (buffer[5] & 0x01) >> 0;
              }
            real_alen += 1;

            if (pcr)
              real_alen += 6;

            if (opcr)
              real_alen += 6;

            if (priv)
              {
                priv_len = buffer[5+real_alen];
                real_alen += 1 + priv_len;
              }

            if (afext)
              {
                afext_len = buffer[5+real_alen];
                real_alen += 1 + afext_len;
              }
          }

        // fprintf (stderr, "pid = 0x%04x, cc = 0x%x\n", pid, cc);

        if (sync != 0x47)
          {
            // TODO, skip to next package
            if (show_packets)
              write (1, "?", 1);

            broken_sync_cnt++;
            goto char_written;
          }

        pidtype = pids[pid];

        if (pidtype == -1)
          {
            pid_cnt++;

            if (pid == 0)
              {
                fprintf (stderr, "# =  pid 0x0000 (PAT)\n");
                pidtype = pids[pid] = 0;
              }
            else if (pid == 0x1fff)
              {
                fprintf (stderr, "\" '  pid 0x1fff (invalid)\n");
                pidtype = pids[pid] = 0;
              }
            else
              {
                if (pid_count < 26)
                  pid_count++;

                pidtype = pids[pid] = pid_count;

                if (show_pids)
                  fprintf (stderr, "%c %c  pid 0x%04x  "
                           "%s%s%s%s%s%s%s%s%s%s%s%salen=%d\n",
                           0x40 + pidtype, 0x60 + pidtype,  pid,
                           priority ? "pri, " : "",
                           adaptation & 0x02 ? "adapt, " : "",
                           adaptation & 0x01 ? "payld, " : "",
                           scrambling > 0 ? "scrmb, " : "",
                           disc > 0 ? "disc, " : "",
                           random > 0 ? "rnd, " : "",
                           espri > 0 ? "ESpri, " : "",
                           pcr > 0 ? "PCR, " : "",
                           opcr > 0 ? "OPCR, " : "",
                           splpt > 0 ? "splpt, " : "",
                           priv > 0 ? "priv, " : "",
                           afext > 0 ? "afext, " : "",
                           alen
                          );
              }

            if ((adaptation == 1 && alen != 0) ||
                (adaptation == 2 && alen != 183) ||
                (adaptation == 3 && alen >= 183))
              {
                fprintf (stderr, "!!! pid 0x%04x: invalid adaptation_field_length %d\n",
                         pid, alen);
              }

            if ((adaptation & 2) != 0 && real_alen > alen )
              {
                fprintf (stderr, "!!! pid 0x%04x: adaptation field to long %d > %d\n",
                         pid, real_alen, alen);
              }

            ccok = 1;
          }
        else
          {
            ccok = ccs[pid] == cc ? 1 : 0;
          }

        if (!ccok)
          unusual_cc_cnt++;

        if ((adaptation == 0x01 && alen != 0) ||
            (adaptation == 0x10 && alen != 183) ||
            (adaptation == 0x11 && alen >= 183) ||
            (alen < real_alen))
          {
            if (show_packets)
              write (1, "@", 1);

            broken_alen_cnt++;
            goto char_written;
          }

        if (show_cc_jumps)
          togglec = !ccok;
        else if (show_pcr)
          togglec = pcr;
        else if (show_start)
          togglec = unit_start;
        else
          togglec = 0;

        if (pid == 0)
          outbuf[0] = togglec ? '#' : '=';
        else if (pid == 0x1fff)
          outbuf[0] = togglec ? '"' : '\'';
        else
          outbuf[0] = 0x40 + pidtype + (togglec ? 0 : 0x20);

        ccs[pid] = (cc + 1) & 0x0F;

        if (show_packets)
          write (1, outbuf, 1);

char_written:
        count++;
        if ((count % 64) == 0 && show_packets)
          write (1, "\n", 1);
      }
    }
}
