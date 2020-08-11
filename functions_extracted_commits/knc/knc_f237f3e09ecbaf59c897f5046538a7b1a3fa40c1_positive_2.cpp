gstd_accept(int fd, char **display_creds, char **export_name, char **mech)
{
	gss_name_t	 client;
	gss_OID		 mech_oid;
	struct gstd_tok *tok;
	gss_ctx_id_t	 ctx = GSS_C_NO_CONTEXT;
	gss_buffer_desc	 in, out;
	OM_uint32	 maj, min;
	int		 ret;

	*display_creds = NULL;
	*export_name = NULL;
	out.length = 0;
	in.length = 0;
	read_packet(fd, &in, 60000, 1);
again:
	while ((ret = read_packet(fd, &in, 60000, 0)) == -2)
		;

	if (ret < 1)
		return NULL;

	maj = gss_accept_sec_context(&min, &ctx, GSS_C_NO_CREDENTIAL,
	    &in, GSS_C_NO_CHANNEL_BINDINGS, &client, &mech_oid, &out, NULL,
	    NULL, NULL);

	gss_release_buffer(&min, &in);

	if (out.length && write_packet(fd, &out)) {
		gss_release_buffer(&min, &out);
		return NULL;
	}
	gss_release_buffer(&min, &out);

	GSTD_GSS_ERROR(maj, min, NULL, "gss_accept_sec_context");

	if (maj & GSS_S_CONTINUE_NEEDED)
		goto again;

	*display_creds = gstd_get_display_name(client);
	*export_name = gstd_get_export_name(client);
	*mech = gstd_get_mech(mech_oid);

	gss_release_name(&min, &client);
	SETUP_GSTD_TOK(tok, ctx, fd, "gstd_accept");
	return tok;
}
read_packet(int fd, gss_buffer_t buf, int timeout, int first)
{
	int	  ret;

	static uint32_t		len = 0;
	static char		len_buf[4];
	static int		len_buf_pos = 0;
	static char *		tmpbuf = 0;
	static int		tmpbuf_pos = 0;

	if (first) {
		len_buf_pos = 0;
		return -2;
	}

	if (len_buf_pos < 4) {
		ret = timed_read(fd, &len_buf[len_buf_pos], 4 - len_buf_pos,
		    timeout);

		if (ret == -1) {
			if (errno == EINTR || errno == EAGAIN)
				return -2;

			LOG(LOG_ERR, ("%s", strerror(errno)));
			goto bail;
		}

		if (ret == 0) {		/* EOF */
			/* Failure to read ANY length just means we're done */
			if (len_buf_pos == 0)
				return 0;

			/*
			 * Otherwise, we got EOF mid-length, and that's
			 * a protocol error.
			 */
			LOG(LOG_INFO, ("EOF reading packet len"));
			goto bail;
		}

		len_buf_pos += ret;
	}

	/* Not done reading the length? */
	if (len_buf_pos != 4)
		return -2;

	/* We have the complete length */
	len = ntohl(*(uint32_t *)len_buf);

	/*
	 * We make sure recvd length is reasonable, allowing for some
	 * slop in enc overhead, beyond the actual maximum number of
	 * bytes of decrypted payload.
	 */
	if (len > GSTD_MAXPACKETCONTENTS + 512) {
		LOG(LOG_ERR, ("ridiculous length, %ld", len));
		goto bail;
	}

	if (!tmpbuf) {
		if ((tmpbuf = malloc(len)) == NULL) {
			LOG(LOG_CRIT, ("malloc failure, %ld bytes", len));
			goto bail;
		}
	}

	ret = timed_read(fd, tmpbuf + tmpbuf_pos, len - tmpbuf_pos, timeout);
	if (ret == -1) {

		if (errno == EINTR || errno == EAGAIN)
			return -2;

		LOG(LOG_ERR, ("%s", strerror(errno)));
		goto bail;
	}

	if (ret == 0) {
		LOG(LOG_ERR, ("EOF while reading packet (len=%d)", len));
		goto bail;
	}

	tmpbuf_pos += ret;

	if (tmpbuf_pos == len) {
		buf->length = len;
		buf->value = tmpbuf;
		len = len_buf_pos = tmpbuf_pos = 0;
		tmpbuf = NULL;

		LOG(LOG_DEBUG, ("read packet of length %d", buf->length));
		return 1;
	}

	return -2;

bail:
	free(tmpbuf);
	tmpbuf = NULL;

	return -1;
}