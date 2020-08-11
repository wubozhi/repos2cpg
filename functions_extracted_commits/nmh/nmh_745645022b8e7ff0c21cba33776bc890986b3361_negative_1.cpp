main (int argc, char **argv)
{
    int chgflag = 1, trnflag = 1;
    int noisy = 1, width = 0;
    int rpop, i, hghnum, msgnum;
    int kpop = 0, sasl = 0;
    char *cp, *maildir, *folder = NULL;
    char *format = NULL, *form = NULL;
    char *host = NULL, *user = NULL;
    char *audfile = NULL, *from = NULL, *saslmech = NULL;
    char buf[BUFSIZ], **argp, *nfs, **arguments;
    struct msgs *mp;
    struct stat st, s1;
    FILE *aud = NULL;
    char	b[MAXPATHLEN + 1];

#ifdef POP
    int nmsgs, nbytes, p = 0;
    char *pass = NULL;
    char *MAILHOST_env_variable;
#endif

#ifdef MHE
    FILE *mhe = NULL;
#endif

#ifdef HESIOD
    struct hes_postoffice *po;
#endif

/* absolutely the first thing we do is save our privileges,
 * and drop them if we can.
 */
    SAVEGROUPPRIVS();
    TRYDROPGROUPPRIVS();

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

#ifdef POP
    /*
     * Scheme is:
     *        use MAILHOST environment variable if present,
     *  else try Hesiod.
     *  If that fails, use the default (if any)
     *  provided by mts.conf in mts_init()
     */
    if ((MAILHOST_env_variable = getenv("MAILHOST")) != NULL)
	pophost = MAILHOST_env_variable;
# ifdef HESIOD
    else if ((po = hes_getmailhost(getusername())) != NULL &&
	     strcmp(po->po_type, "POP") == 0)
	pophost = po->po_host;
# endif /* HESIOD */
    /*
     * If there is a valid "pophost" entry in mts.conf,
     * then use it as the default host.
     */
    if (pophost && *pophost)
	host = pophost;

    if ((cp = getenv ("MHPOPDEBUG")) && *cp)
	snoop++;
#endif /* POP */

    rpop = 0;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW: 
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW: 
		adios (NULL, "-%s unknown", cp);

	    case HELPSW: 
		snprintf (buf, sizeof(buf), "%s [+folder] [switches]", invo_name);
		print_help (buf, switches, 1);
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

	    case AUDSW: 
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		audfile = getcpy (m_maildir (cp));
		continue;
	    case NAUDSW: 
		audfile = NULL;
		continue;

	    case CHGSW: 
		chgflag++;
		continue;
	    case NCHGSW: 
		chgflag = 0;
		continue;

	    /*
	     * The flag `trnflag' has the value:
	     *
	     * 2 if -truncate is given
	     * 1 by default (truncating is default)
	     * 0 if -notruncate is given
	     */
	    case TRNCSW: 
		trnflag = 2;
		continue;
	    case NTRNCSW: 
		trnflag = 0;
		continue;

	    case FILESW: 
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		from = path (cp, TFILE);

		/*
		 * If the truncate file is in default state,
		 * change to not truncate.
		 */
		if (trnflag == 1)
		    trnflag = 0;
		continue;

	    case SILSW: 
		noisy = 0;
		continue;
	    case NSILSW: 
		noisy++;
		continue;

	    case FORMSW: 
		if (!(form = *argp++) || *form == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		format = NULL;
		continue;
	    case FMTSW: 
		if (!(format = *argp++) || *format == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		form = NULL;
		continue;

	    case WIDTHSW: 
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		width = atoi (cp);
		continue;

	    case HOSTSW:
		if (!(host = *argp++) || *host == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case USERSW:
		if (!(user = *argp++) || *user == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case PACKSW:
#ifndef	POP
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
#else /* POP */
		if (!(packfile = *argp++) || *packfile == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
#endif /* POP */
		continue;
	    case NPACKSW:
#ifdef POP
		packfile = NULL;
#endif /* POP */
		continue;

	    case APOPSW:
		rpop = -1;
		continue;
	    case NAPOPSW:
		rpop = 0;
		continue;

	    case RPOPSW:
		rpop = 1;
		continue;
	    case NRPOPSW:
		rpop = 0;
		continue;

	    case KPOPSW:
		kpop = 1;
		continue;

	    case SNOOPSW:
		snoop++;
		continue;
	
	    case SASLSW:
		sasl++;
		continue;
	
	    case SASLMECHSW:
		if (!(saslmech = *argp++) || *saslmech == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    adios (NULL, "usage: %s [+folder] [switches]", invo_name);
	}
    }

    /* NOTE: above this point you should use TRYDROPGROUPPRIVS(),
     * not DROPGROUPPRIVS().
     */
#ifdef POP
    if (host && !*host)
	host = NULL;
    if (from || !host || rpop <= 0)
	DROPUSERPRIVS();
#endif /* POP */

    /* guarantee dropping group priveleges; we might not have done so earlier */
    DROPGROUPPRIVS();

    /*
     * Where are we getting the new mail?
     */
    if (from)
	inc_type = INC_FILE;
#ifdef POP
    else if (host)
	inc_type = INC_POP;
#endif
    else
	inc_type = INC_FILE;

#ifdef POP
    /*
     * Are we getting the mail from
     * a POP server?
     */
    if (inc_type == INC_POP) {
	if (user == NULL)
	    user = getusername ();
	if ( strcmp( POPSERVICE, "kpop" ) == 0 ) {
	    kpop = 1;
	}
	if (kpop || sasl || ( rpop > 0))
	    pass = getusername ();
	else
	    ruserpass (host, &user, &pass);

	/*
	 * initialize POP connection
	 */
	if (pop_init (host, user, pass, snoop, kpop ? 1 : rpop, kpop,
		      sasl, saslmech) == NOTOK)
	    adios (NULL, "%s", response);

	/* Check if there are any messages */
	if (pop_stat (&nmsgs, &nbytes) == NOTOK)
	    adios (NULL, "%s", response);

	if (rpop > 0)
	    DROPUSERPRIVS();
	if (nmsgs == 0) {
	    pop_quit();
	    adios (NULL, "no mail to incorporate");
	}
    }
#endif /* POP */

    /*
     * We will get the mail from a file
     * (typically the standard maildrop)
     */

    if (inc_type == INC_FILE) {
	if (from)
	    newmail = from;
	else if ((newmail = getenv ("MAILDROP")) && *newmail)
	    newmail = m_mailpath (newmail);
	else if ((newmail = context_find ("maildrop")) && *newmail)
	    newmail = m_mailpath (newmail);
	else {
	    newmail = concat (MAILDIR, "/", MAILFIL, NULL);
	}
	if (stat (newmail, &s1) == NOTOK || s1.st_size == 0)
	    adios (NULL, "no mail to incorporate");
    }

#ifdef POP
    /* skip the folder setup */
    if ((inc_type == INC_POP) && packfile)
	goto go_to_it;
#endif /* POP */

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!folder)
	folder = getfolder (0);
    maildir = m_maildir (folder);

    if (stat (maildir, &st) == NOTOK) {
	if (errno != ENOENT)
	    adios (maildir, "error on folder");
	cp = concat ("Create folder \"", maildir, "\"? ", NULL);
	if (noisy && !getanswer (cp))
	    done (1);
	free (cp);
	if (!makedir (maildir))
	    adios (NULL, "unable to create folder %s", maildir);
    }

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

#ifdef POP
go_to_it:
#endif /* POP */

    if (inc_type == INC_FILE) {
	if (access (newmail, W_OK) != NOTOK) {
	    locked++;
	    if (trnflag) {
		SIGNAL (SIGHUP, SIG_IGN);
		SIGNAL (SIGINT, SIG_IGN);
		SIGNAL (SIGQUIT, SIG_IGN);
		SIGNAL (SIGTERM, SIG_IGN);
	    }

            GETGROUPPRIVS();       /* Reset gid to lock mail file */
            in = lkfopen (newmail, "r");
            DROPGROUPPRIVS();
            if (in == NULL)
		adios (NULL, "unable to lock and fopen %s", newmail);
	    fstat (fileno(in), &s1);
	} else {
	    trnflag = 0;
	    if ((in = fopen (newmail, "r")) == NULL)
		adios (newmail, "unable to read");
	}
    }

    /* This shouldn't be necessary but it can't hurt. */
    DROPGROUPPRIVS();

    if (audfile) {
	if ((i = stat (audfile, &st)) == NOTOK)
	    advise (NULL, "Creating Receive-Audit: %s", audfile);
	if ((aud = fopen (audfile, "a")) == NULL)
	    adios (audfile, "unable to append to");
	else if (i == NOTOK)
	    chmod (audfile, m_gmprot ());

#ifdef POP
	fprintf (aud, from ? "<<inc>> %s -ms %s\n"
		 : host ? "<<inc>> %s -host %s -user %s%s\n"
		 : "<<inc>> %s\n",
		 dtimenow (0), from ? from : host, user,
		 rpop < 0 ? " -apop" : rpop > 0 ? " -rpop" : "");
#else /* POP */
	fprintf (aud, from ? "<<inc>> %s  -ms %s\n" : "<<inc>> %s\n",
		 dtimenow (0), from);
#endif /* POP */
    }

#ifdef MHE
    if (context_find ("mhe")) {
	cp = concat (maildir, "/++", NULL);
	i = stat (cp, &st);
	if ((mhe = fopen (cp, "a")) == NULL)
	    admonish (cp, "unable to append to");
	else
	    if (i == NOTOK)
		chmod (cp, m_gmprot ());
	free (cp);
    }
#endif /* MHE */

    /* Get new format string */
    nfs = new_fs (form, format, FORMAT);

    if (noisy) {
	printf ("Incorporating new mail into %s...\n\n", folder);
	fflush (stdout);
    }

#ifdef POP
    /*
     * Get the mail from a POP server
     */
    if (inc_type == INC_POP) {
	if (packfile) {
	    packfile = path (packfile, TFILE);
	    if (stat (packfile, &st) == NOTOK) {
		if (errno != ENOENT)
		    adios (packfile, "error on file");
		cp = concat ("Create file \"", packfile, "\"? ", NULL);
		if (noisy && !getanswer (cp))
		    done (1);
		free (cp);
	    }
	    msgnum = map_count ();
	    if ((pd = mbx_open (packfile, mbx_style, getuid(), getgid(), m_gmprot()))
		== NOTOK)
		adios (packfile, "unable to open");
	    if ((pf = fdopen (pd, "w+")) == NULL)
		adios (NULL, "unable to fdopen %s", packfile);
	} else {
	    hghnum = msgnum = mp->hghmsg;
	    /*
	     * Check if we have enough message space for all the new
	     * messages.  If not, then realloc the folder and add enough
	     * space for all new messages plus 10 additional slots.
	     */
	    if (mp->hghmsg + nmsgs >= mp->hghoff
		&& !(mp = folder_realloc (mp, mp->lowoff, mp->hghmsg + nmsgs + 10)))
		adios (NULL, "unable to allocate folder storage");
	}

	for (i = 1; i <= nmsgs; i++) {
	    msgnum++;
	    if (packfile) {
		fseek (pf, 0L, SEEK_CUR);
		pos = ftell (pf);
		size = 0;
		fwrite (mmdlm1, 1, strlen (mmdlm1), pf);
		start = ftell (pf);

		if (pop_retr (i, pop_pack) == NOTOK)
		    adios (NULL, "%s", response);

		fseek (pf, 0L, SEEK_CUR);
		stop = ftell (pf);
		if (fflush (pf))
		    adios (packfile, "write error on");
		fseek (pf, start, SEEK_SET);
	    } else {
		cp = getcpy (m_name (msgnum));
		if ((pf = fopen (cp, "w+")) == NULL)
		    adios (cp, "unable to write");
		chmod (cp, m_gmprot ());
		start = stop = 0L;

		if (pop_retr (i, pop_action) == NOTOK)
		    adios (NULL, "%s", response);

		if (fflush (pf))
		    adios (cp, "write error on");
		fseek (pf, 0L, SEEK_SET);
	    }
	    switch (p = scan (pf, msgnum, 0, nfs, width,
			      packfile ? 0 : msgnum == mp->hghmsg + 1 && chgflag,
			      1, NULL, stop - start, noisy)) {
	    case SCNEOF: 
		printf ("%*d  empty\n", DMAXFOLDER, msgnum);
		break;

	    case SCNFAT:
		trnflag = 0;
		noisy++;
		/* advise (cp, "unable to read"); already advised */
		/* fall thru */

	    case SCNERR:
	    case SCNNUM: 
		break;

	    case SCNMSG: 
	    case SCNENC:
	    default: 
		if (aud)
		    fputs (scanl, aud);
# ifdef MHE
		if (mhe)
		    fputs (scanl, mhe);
# endif /* MHE */
		if (noisy)
		    fflush (stdout);
		if (!packfile) {
		    clear_msg_flags (mp, msgnum);
		    set_exists (mp, msgnum);
		    set_unseen (mp, msgnum);
		    mp->msgflags |= SEQMOD;
		}
		break;
	    }
	    if (packfile) {
		fseek (pf, stop, SEEK_SET);
		fwrite (mmdlm2, 1, strlen (mmdlm2), pf);
		if (fflush (pf) || ferror (pf)) {
		    int e = errno;
		    pop_quit ();
		    errno = e;
		    adios (packfile, "write error on");
		}
		map_write (packfile, pd, 0, 0L, start, stop, pos, size, noisy);
	    } else {
		if (ferror(pf) || fclose (pf)) {
		    int e = errno;
		    unlink (cp);
		    pop_quit ();
		    errno = e;
		    adios (cp, "write error on");
		}
		free (cp);
	    }

	    if (trnflag && pop_dele (i) == NOTOK)
		adios (NULL, "%s", response);
	}

	if (pop_quit () == NOTOK)
	    adios (NULL, "%s", response);
	if (packfile) {
	    mbx_close (packfile, pd);
	    pd = NOTOK;
	}
    }
#endif /* POP */

    /*
     * Get the mail from file (usually mail spool)
     */
    if (inc_type == INC_FILE) {
	m_unknown (in);		/* the MAGIC invocation... */
	hghnum = msgnum = mp->hghmsg;
	for (i = 0;;) {
	    /*
	     * Check if we need to allocate more space for message status.
	     * If so, then add space for an additional 100 messages.
	     */
	    if (msgnum >= mp->hghoff
		&& !(mp = folder_realloc (mp, mp->lowoff, mp->hghoff + 100))) {
		advise (NULL, "unable to allocate folder storage");
		i = NOTOK;
		break;
	    }

#if 0
	    /* copy file from spool to tmp file */
	    tmpfilenam = m_scratch ("", invo_name);
	    if ((fd = creat (tmpfilenam, m_gmprot ())) == NOTOK)
		adios (tmpfilenam, "unable to create");
	    chmod (tmpfilenam, m_gmprot ());
	    if (!(in2 = fdopen (fd, "r+")))
		adios (tmpfilenam, "unable to access");
	    cpymsg (in, in2);

	    /* link message into folder */
	    newmsg = folder_addmsg(mp, tmpfilenam);
#endif
	    /* create scanline for new message */
	    switch (i = scan (in, msgnum + 1, msgnum + 1, nfs, width,
			      msgnum == hghnum && chgflag, 1, NULL, 0L, noisy)) {
	    case SCNFAT:
	    case SCNEOF: 
		break;

	    case SCNERR:
		if (aud)
		    fputs ("inc aborted!\n", aud);
		advise (NULL, "aborted!");	/* doesn't clean up locks! */
		break;

	    case SCNNUM: 
		advise (NULL, "BUG in %s, number out of range", invo_name);
		break;

	    default: 
		advise (NULL, "BUG in %s, scan() botch (%d)", invo_name, i);
		break;

	    case SCNMSG:
	    case SCNENC:
		/*
		 *  Run the external program hook on the message.
		 */

		(void)snprintf(b, sizeof (b), "%s/%d", maildir, msgnum + 1);
		(void)ext_hook("add-hook", b, (char *)0);

		if (aud)
		    fputs (scanl, aud);
#ifdef MHE
		if (mhe)
		    fputs (scanl, mhe);
#endif /* MHE */
		if (noisy)
		    fflush (stdout);

		msgnum++;
		mp->hghmsg++;
		mp->nummsg++;
		if (mp->lowmsg == 0) mp->lowmsg = 1;

		clear_msg_flags (mp, msgnum);
		set_exists (mp, msgnum);
		set_unseen (mp, msgnum);
		mp->msgflags |= SEQMOD;
		continue;
	    }
	    break;
	}
    }

#ifdef POP
    if (p < 0) {		/* error */
#else
    if (i < 0) {		/* error */
#endif
	if (locked) {
            GETGROUPPRIVS();      /* Be sure we can unlock mail file */
            (void) lkfclose (in, newmail); in = NULL;
            DROPGROUPPRIVS();    /* And then return us to normal privileges */
	} else {
	    fclose (in); in = NULL;
	}
	adios (NULL, "failed");
    }

    if (aud)
	fclose (aud);

#ifdef MHE
    if (mhe)
	fclose (mhe);
#endif /* MHE */

    if (noisy)
	fflush (stdout);

#ifdef POP
    if ((inc_type == INC_POP) && packfile)
	done (0);
#endif /* POP */

    /*
     * truncate file we are incorporating from
     */
    if (inc_type == INC_FILE) {
	if (trnflag) {
	    if (stat (newmail, &st) != NOTOK && s1.st_mtime != st.st_mtime)
		advise (NULL, "new messages have arrived!\007");
	    else {
		if ((i = creat (newmail, 0600)) != NOTOK)
		    close (i);
		else
		    admonish (newmail, "error zero'ing");
		unlink(map_name(newmail));
	    }
	} else {
	    if (noisy)
		printf ("%s not zero'd\n", newmail);
	}
    }

    if (msgnum == hghnum) {
	admonish (NULL, "no messages incorporated");
    } else {
	context_replace (pfolder, folder);	/* update current folder */
	if (chgflag)
	    mp->curmsg = hghnum + 1;
	mp->hghmsg = msgnum;
	if (mp->lowmsg == 0)
	    mp->lowmsg = 1;
	if (chgflag)		/* sigh... */
	    seq_setcur (mp, mp->curmsg);
    }

    /*
     * unlock the mail spool
     */
    if (inc_type == INC_FILE) {
	if (locked) {
           GETGROUPPRIVS();        /* Be sure we can unlock mail file */
           (void) lkfclose (in, newmail); in = NULL;
           DROPGROUPPRIVS();       /* And then return us to normal privileges */
	} else {
	    fclose (in); in = NULL;
	}
    }

    seq_setunseen (mp, 0);	/* set the Unseen-Sequence */
    seq_save (mp);		/* synchronize sequences   */
    context_save ();		/* save the context file   */
    return done (0);
}