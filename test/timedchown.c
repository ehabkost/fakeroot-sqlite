/* Small utility to test ordering and timing on fakeroot
 *
 * Copyright (c) 2009 Eduardo Habkost <ehabkost@raisama.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>


FILE *reply_fifo;

void doit(const char *file, uid_t uid, uid_t uid2, int count)
{
	int i;
	char reply[256];

	fprintf(stderr, "dummy chown requests:\n");
	fflush(stderr);

	/* first, flood faked with other calls */
	for (i = 0; i < count; i++) {
		if (chown(file, uid2, -1) < 0)
			exit(1);
	}

	fprintf(stderr, "real chown request:\n");
	fflush(stderr);

	/* now, send the real chown call: */
	if (chown(file, uid, -1) < 0)
		exit(1);

	fprintf(stderr, "uid changed to %d. notifying.\n", (int)uid);
	fflush(stderr);

	/* tell the other side we are done */
	printf("%d\n", (int)uid);
	fflush(stdout);

	/* wait for the other side to reply */
	if (!fgets(reply, 256, reply_fifo))
		exit(1);

	fprintf(stderr, "got reply: %s\n", reply);
	fflush(stderr);

}

int main(int argc, const char *argv[])
{
	const char *file;
	uid_t uid1, uid2;
	int floodcount, loopcount, i;

	if (argc < 6)
		return 1;

	uid1 = atoi(argv[1]);
	uid2 = atoi(argv[2]);
	file = argv[3];
	floodcount = atoi(argv[4]);
	loopcount = atoi(argv[5]);
	reply_fifo = fopen(argv[6], "r");
	if (!reply_fifo)
		return 1;

	for (i = 0; i < loopcount; i++) {
		/* alternate between uid1 and uid2 */
		doit(file, uid1, uid2, floodcount);
		doit(file, uid2, uid1, floodcount);
	}
}
