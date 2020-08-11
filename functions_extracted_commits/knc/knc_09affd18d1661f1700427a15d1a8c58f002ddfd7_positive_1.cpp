knc_loop(knc_ctx ctx, int server)
{
	int	 i;
	int	 loopcount = 0;
	int	 ret;
	int	 do_recv = 1;
	int	 do_send = 1;
	int	 valrecv = 0;
	int	 valsend = 0;
	char	*buf;

	knc_set_opt(ctx, KNC_SOCK_NONBLOCK, 1);

	for (;;) {
		knc_callback	cbs[2];
		struct pollfd	fds[4];
		nfds_t		nfds;

		fprintf(stderr, "%s: loop start (% 6d), "
		    "R=% 9d %s S=% 9d %s ToSend=% 9zu\n", server?"S":"C",
		    ++loopcount, valrecv, do_recv?"    ":"done", valsend,
		    do_send?"    ":"done", knc_pending(ctx, KNC_DIR_SEND));

		if (knc_error(ctx))
			break;

		if (knc_io_complete(ctx))
			break;

		/*
		 * The data that we are sending and receiving is a simple
		 * steam of incrementing single byte integers modulo 11.
		 * Both sides send the same data, so it can be validated.
		 */

		if (do_send && knc_pending(ctx, KNC_DIR_SEND) < UNIT_BUFSIZ) {
			ret = knc_get_ibuf(ctx, KNC_DIR_SEND,
			    (void **)&buf, 8192);
			if (ret == -1)
				fprintf(stderr, "%d: ret == -1 for sending\n",
				    getpid());

			for (i=0; i < ret; i++)
				buf[i] = valsend++ % 11;

			if (ret > 0)
				knc_fill_buf(ctx, KNC_DIR_SEND, ret);
		}

		while (knc_pending(ctx, KNC_DIR_RECV) > 0) {
			ret = knc_get_obuf(ctx, KNC_DIR_RECV,
			    (void **)&buf, 8192);
			if (ret <= 0)
				break;

			for (i=0; i < ret; i++) {
				if (buf[i] != valrecv++ % 11) {
					fprintf(stderr, "Malformed input\n");
					return -1;
				}
			}

			knc_drain_buf(ctx, KNC_DIR_RECV, ret);
		}

		if (valrecv >= TEST_SIZE)
			do_recv = 0;

		if (do_send && valsend >= TEST_SIZE) {
			knc_put_eof(ctx, KNC_DIR_SEND);
			do_send = 0;
		}


		nfds = knc_get_pollfds(ctx, fds, cbs, 4);

		ret = poll(fds, nfds, 0);
		if (ret < 0) {
			fprintf(stderr, "poll: %s\n", strerror(errno));
			break;
		}

		knc_service_pollfds(ctx, fds, cbs, nfds);
	}

	fprintf(stderr, "%s: loop done  (% 6d), "
	    "R=% 9d %s S=% 9d %s ToSend=% 9zu\n", server?"S":"C",
	    ++loopcount, valrecv, do_recv?"    ":"done", valsend,
	    do_send?"    ":"done", knc_pending(ctx, KNC_DIR_SEND));

	ret = 0;
	if (knc_error(ctx)) {
		fprintf(stderr, "%s: KNC UNIT TEST ERROR: %s\n",
		    server?"S":"C", knc_errstr(ctx));
		ret = 1;
	}

	knc_close(ctx);
	knc_ctx_close(ctx);
	return ret;
}