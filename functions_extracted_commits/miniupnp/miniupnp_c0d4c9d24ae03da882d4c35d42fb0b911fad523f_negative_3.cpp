AddAnyPortMapping(struct upnphttp * h, const char * action)
{
	int r;
	static const char resp[] =
		"<u:%sResponse "
		"xmlns:u=\"%s\">"
		"<NewReservedPort>%hu</NewReservedPort>"
		"</u:%sResponse>";

	char body[512];
	int bodylen;

	struct NameValueParserData data;
	const char * int_ip, * int_port, * ext_port, * protocol, * desc;
	const char * r_host;
	unsigned short iport, eport;
	const char * leaseduration_str;
	unsigned int leaseduration;

	struct hostent *hp; /* getbyhostname() */
	char ** ptr; /* getbyhostname() */
	struct in_addr result_ip;/*unsigned char result_ip[16];*/ /* inet_pton() */

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);
	r_host = GetValueFromNameValueList(&data, "NewRemoteHost");
	ext_port = GetValueFromNameValueList(&data, "NewExternalPort");
	protocol = GetValueFromNameValueList(&data, "NewProtocol");
	int_port = GetValueFromNameValueList(&data, "NewInternalPort");
	int_ip = GetValueFromNameValueList(&data, "NewInternalClient");
	/* NewEnabled */
	desc = GetValueFromNameValueList(&data, "NewPortMappingDescription");
	leaseduration_str = GetValueFromNameValueList(&data, "NewLeaseDuration");

	leaseduration = leaseduration_str ? atoi(leaseduration_str) : 0;
	if(leaseduration == 0)
		leaseduration = 604800;

	eport = (unsigned short)atoi(ext_port);
	iport = (unsigned short)atoi(int_port);

	if (!int_ip)
	{
		ClearNameValueList(&data);
		SoapError(h, 402, "Invalid Args");
		return;
	}
#ifndef SUPPORT_REMOTEHOST
#ifdef UPNP_STRICT
	if (r_host && (strlen(r_host) > 0) && (0 != strcmp(r_host, "*")))
	{
		ClearNameValueList(&data);
		SoapError(h, 726, "RemoteHostOnlySupportsWildcard");
		return;
	}
#endif
#endif

	/* if ip not valid assume hostname and convert */
	if (inet_pton(AF_INET, int_ip, &result_ip) <= 0)
	{
		hp = gethostbyname(int_ip);
		if(hp && hp->h_addrtype == AF_INET)
		{
			for(ptr = hp->h_addr_list; ptr && *ptr; ptr++)
		   	{
				int_ip = inet_ntoa(*((struct in_addr *) *ptr));
				result_ip = *((struct in_addr *) *ptr);
				/* TODO : deal with more than one ip per hostname */
				break;
			}
		}
		else
		{
			syslog(LOG_ERR, "Failed to convert hostname '%s' to ip address", int_ip);
			ClearNameValueList(&data);
			SoapError(h, 402, "Invalid Args");
			return;
		}
	}

	/* check if NewInternalAddress is the client address */
	if(GETFLAG(SECUREMODEMASK))
	{
		if(h->clientaddr.s_addr != result_ip.s_addr)
		{
			syslog(LOG_INFO, "Client %s tried to redirect port to %s",
			       inet_ntoa(h->clientaddr), int_ip);
			ClearNameValueList(&data);
			SoapError(h, 606, "Action not authorized");
			return;
		}
	}

	/* TODO : accept a different external port
	 * have some smart strategy to choose the port */
	for(;;) {
		r = upnp_redirect(r_host, eport, int_ip, iport, protocol, desc, leaseduration);
		if(r==-2 && eport < 65535) {
			eport++;
		} else {
			break;
		}
	}

	ClearNameValueList(&data);

	switch(r)
	{
	case 0:	/* success */
		bodylen = snprintf(body, sizeof(body), resp,
		              action, SERVICE_TYPE_WANIPC,
					  eport, action);
		BuildSendAndCloseSoapResp(h, body, bodylen);
		break;
	case -2:	/* already redirected */
		SoapError(h, 718, "ConflictInMappingEntry");
		break;
	case -3:	/* not permitted */
		SoapError(h, 606, "Action not authorized");
		break;
	default:
		SoapError(h, 501, "ActionFailed");
	}
}
DeletePortMappingRange(struct upnphttp * h, const char * action)
{
	int r = -1;
	static const char resp[] =
		"<u:DeletePortMappingRangeResponse "
		"xmlns:u=\"" SERVICE_TYPE_WANIPC "\">"
		"</u:DeletePortMappingRangeResponse>";
	struct NameValueParserData data;
	const char * protocol;
	unsigned short startport, endport;
	int manage;
	unsigned short * port_list;
	unsigned int i, number = 0;
	UNUSED(action);

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);
	startport = (unsigned short)atoi(GetValueFromNameValueList(&data, "NewStartPort"));
	endport = (unsigned short)atoi(GetValueFromNameValueList(&data, "NewEndPort"));
	protocol = GetValueFromNameValueList(&data, "NewProtocol");
	manage = atoi(GetValueFromNameValueList(&data, "NewManage"));

	/* possible errors :
	   606 - Action not authorized
	   730 - PortMappingNotFound
	   733 - InconsistentParameter
	 */
	if(startport > endport)
	{
		SoapError(h, 733, "InconsistentParameter");
		ClearNameValueList(&data);
		return;
	}

	port_list = upnp_get_portmappings_in_range(startport, endport,
	                                           protocol, &number);
	for(i = 0; i < number; i++)
	{
		r = upnp_delete_redirection(port_list[i], protocol);
		/* TODO : check return value for errors */
	}
	free(port_list);
	BuildSendAndCloseSoapResp(h, resp, sizeof(resp)-1);

	ClearNameValueList(&data);
}
GetListOfPortMappings(struct upnphttp * h, const char * action)
{
	static const char resp_start[] =
		"<u:%sResponse "
		"xmlns:u=\"%s\">"
		"<NewPortListing><![CDATA[";
	static const char resp_end[] =
		"]]></NewPortListing>"
		"</u:%sResponse>";

	static const char list_start[] =
		"<p:PortMappingList xmlns:p=\"urn:schemas-upnp-org:gw:WANIPConnection\""
		" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
		" xsi:schemaLocation=\"urn:schemas-upnp-org:gw:WANIPConnection"
		" http://www.upnp.org/schemas/gw/WANIPConnection-v2.xsd\">";
	static const char list_end[] =
		"</p:PortMappingList>";

	static const char entry[] =
		"<p:PortMappingEntry>"
		"<p:NewRemoteHost>%s</p:NewRemoteHost>"
		"<p:NewExternalPort>%hu</p:NewExternalPort>"
		"<p:NewProtocol>%s</p:NewProtocol>"
		"<p:NewInternalPort>%hu</p:NewInternalPort>"
		"<p:NewInternalClient>%s</p:NewInternalClient>"
		"<p:NewEnabled>1</p:NewEnabled>"
		"<p:NewDescription>%s</p:NewDescription>"
		"<p:NewLeaseTime>%u</p:NewLeaseTime>"
		"</p:PortMappingEntry>";

	char * body;
	size_t bodyalloc;
	int bodylen;

	int r = -1;
	unsigned short iport;
	char int_ip[32];
	char desc[64];
	char rhost[64];
	unsigned int leaseduration = 0;

	struct NameValueParserData data;
	unsigned short startport, endport;
	const char * protocol;
	int manage;
	int number;
	unsigned short * port_list;
	unsigned int i, list_size = 0;

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);
	startport = (unsigned short)atoi(GetValueFromNameValueList(&data, "NewStartPort"));
	endport = (unsigned short)atoi(GetValueFromNameValueList(&data, "NewEndPort"));
	protocol = GetValueFromNameValueList(&data, "NewProtocol");
	manage = atoi(GetValueFromNameValueList(&data, "NewManage"));
	number = atoi(GetValueFromNameValueList(&data, "NewNumberOfPorts"));
	if(number == 0) number = 1000;	/* return up to 1000 mappings by default */

	if(startport > endport)
	{
		SoapError(h, 733, "InconsistentParameter");
		ClearNameValueList(&data);
		return;
	}
/*
build the PortMappingList xml document :

<p:PortMappingList xmlns:p="urn:schemas-upnp-org:gw:WANIPConnection"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
xsi:schemaLocation="urn:schemas-upnp-org:gw:WANIPConnection
http://www.upnp.org/schemas/gw/WANIPConnection-v2.xsd">
<p:PortMappingEntry>
<p:NewRemoteHost>202.233.2.1</p:NewRemoteHost>
<p:NewExternalPort>2345</p:NewExternalPort>
<p:NewProtocol>TCP</p:NewProtocol>
<p:NewInternalPort>2345</p:NewInternalPort>
<p:NewInternalClient>192.168.1.137</p:NewInternalClient>
<p:NewEnabled>1</p:NewEnabled>
<p:NewDescription>dooom</p:NewDescription>
<p:NewLeaseTime>345</p:NewLeaseTime>
</p:PortMappingEntry>
</p:PortMappingList>
*/
	bodyalloc = 4096;
	body = malloc(bodyalloc);
	if(!body)
	{
		ClearNameValueList(&data);
		SoapError(h, 501, "ActionFailed");
		return;
	}
	bodylen = snprintf(body, bodyalloc, resp_start,
	              action, SERVICE_TYPE_WANIPC);
	if(bodylen < 0)
	{
		SoapError(h, 501, "ActionFailed");
		return;
	}
	memcpy(body+bodylen, list_start, sizeof(list_start));
	bodylen += (sizeof(list_start) - 1);

	port_list = upnp_get_portmappings_in_range(startport, endport,
	                                           protocol, &list_size);
	/* loop through port mappings */
	for(i = 0; number > 0 && i < list_size; i++)
	{
		/* have a margin of 1024 bytes to store the new entry */
		if((unsigned int)bodylen + 1024 > bodyalloc)
		{
			bodyalloc += 4096;
			body = realloc(body, bodyalloc);
			if(!body)
			{
				ClearNameValueList(&data);
				SoapError(h, 501, "ActionFailed");
				free(port_list);
				return;
			}
		}
		rhost[0] = '\0';
		r = upnp_get_redirection_infos(port_list[i], protocol, &iport,
		                               int_ip, sizeof(int_ip),
		                               desc, sizeof(desc),
		                               rhost, sizeof(rhost),
		                               &leaseduration);
		if(r == 0)
		{
			bodylen += snprintf(body+bodylen, bodyalloc-bodylen, entry,
			                    rhost, port_list[i], protocol,
			                    iport, int_ip, desc, leaseduration);
			number--;
		}
	}
	free(port_list);
	port_list = NULL;

	memcpy(body+bodylen, list_end, sizeof(list_end));
	bodylen += (sizeof(list_end) - 1);
	bodylen += snprintf(body+bodylen, bodyalloc-bodylen, resp_end,
	                    action);
	BuildSendAndCloseSoapResp(h, body, bodylen);
	free(body);

	ClearNameValueList(&data);
}