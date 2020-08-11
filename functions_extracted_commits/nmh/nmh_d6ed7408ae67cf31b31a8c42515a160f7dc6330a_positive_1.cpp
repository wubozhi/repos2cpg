client (char *args, char *service, char *response, int len_response, int debug)
{
    int sd, rc;
    char **ap, *arguments[MAXARGS];
    struct addrinfo hints, *res, *ai;

    ap = arguments;
    if (args != NULL && *args != 0) {
	ap = client_copyip (client_brkstring (client_getcpy (args), " ", "\n"),
		ap, MAXARGS);
    } else {
	if (servers != NULL && *servers != 0)
	    ap = client_copyip (client_brkstring (client_getcpy (servers), " ", "\n"),
		ap, MAXARGS);
    }
    if (ap == arguments) {
	*ap++ = client_getcpy ("localhost");
	*ap = NULL;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    for (ap = arguments; *ap; ap++) {

	if (debug) {
	    fprintf(stderr, "Trying to connect to \"%s\" ...\n", *ap);
	}

	rc = getaddrinfo(*ap, service, &hints, &res);

	if (rc) {
	    if (debug) {
		fprintf(stderr, "Lookup of \"%s\" failed: %s\n", *ap,
			gai_strerror(rc));
	    }
	    continue;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {
	    if (debug) {
		char address[NI_MAXHOST];

		rc = getnameinfo(ai->ai_addr, ai->ai_addrlen, address,
				 sizeof(address), NULL, NULL, NI_NUMERICHOST);

		fprintf(stderr, "Connecting to %s...\n",
			rc ? "unknown" : address);
	    }

	    sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

	    if (sd < 0) {
		if (debug)
		    fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		continue;
	    }

	    if (connect(sd, ai->ai_addr, ai->ai_addrlen) == 0) {
		freeaddrinfo(res);
		client_freelist(ap);
		return sd;
	    }

	    if (debug) {
		fprintf(stderr, "Connection failed: %s\n", strerror(errno));
	    }

	    close(sd);
	}

    	freeaddrinfo(res);
    }

    client_freelist(ap);
    strncpy (response, "no servers available", len_response);
    return NOTOK;
}