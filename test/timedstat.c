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


FILE *reply_fifo;

int main(int argc, const char *argv[])
{
	const char *filename;

	if (argc < 1)
		return 1;

	filename = argv[1];
	reply_fifo = fopen(argv[2], "w");
	if (!reply_fifo)
		return 1;

	while (1) {
		uid_t uid;
		char buf[256];

		if (!fgets(buf, 256, stdin))
			return 0;

		uid = atoi(buf);

		struct stat st;
		if (stat(argv[1], &st) < 0)
			return 1;

		printf("expected uid: %d. mode: 0%o, uid: %d\n", (int)uid, (int)st.st_mode, (int)st.st_uid);

		if (uid != st.st_uid) {
			fprintf(stderr, "FAIL: %d != %d\n", (int)uid, (int)st.st_uid);
			return 1;
		}

		fprintf(reply_fifo, "ok\n");
		fflush(reply_fifo);
	}
}
