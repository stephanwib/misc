/*
 * "saveppd" command for CUPS.
 *
 * Copyright 2007-2016 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#define _CUPS_NO_DEPRECATED
#define _PPD_DEPRECATED
#include <cups/cups-private.h>
#include <cups/ppd-private.h>


/*
 * Local functions...
 */
static char		*get_printer_ppd(const char *uri, char *buffer, size_t bufsize);
static int    copy_file(const char *from, const char *to);

/*
 * 'main()' - Parse options and create the PPD.
 */

int
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{

  char		*file,		/* New PPD file */
		evefile[1024] = ""; /* IPP Everywhere PPD */
  const char	*device_uri;	/* device-uri value */

  if (argc != 3)
  {
    _cupsLangPrintf(stderr, "Usage: %s <File> <URI>", argv[0]);
		return (1);
  }
  
  device_uri = argv[2];
  if ((file = get_printer_ppd(device_uri, evefile, sizeof(evefile))) == NULL)
  {
    _cupsLangPrintf(stderr, "Cannot save PPD file.");
    return (1);
  }

  if(copy_file(file, argv[1]) < 0)
  {
  	_cupsLangPrintf(stderr, "Cannot copy PPD file.");
    return (1);
  }
  

  if (evefile[0])
    unlink(evefile);

  return (0);


}


/*
 * 'get_printer_ppd()' - Get an IPP Everywhere PPD file for the given URI.
 */

static char *				/* O - Filename or NULL */
get_printer_ppd(const char *uri,	/* I - Printer URI */
                char       *buffer,	/* I - Filename buffer */
		size_t     bufsize)	/* I - Size of filename buffer */
{
  http_t	*http;			/* Connection to printer */
  ipp_t		*request,		/* Get-Printer-Attributes request */
		*response;		/* Get-Printer-Attributes response */
  char		resolved[1024],		/* Resolved URI */
		scheme[32],		/* URI scheme */
		userpass[256],		/* Username:password */
		host[256],		/* Hostname */
		resource[256];		/* Resource path */
  int		port;			/* Port number */


 /*
  * Connect to the printer...
  */

  if (strstr(uri, "._tcp"))
  {
   /*
    * Resolve URI...
    */

    if (!_httpResolveURI(uri, resolved, sizeof(resolved), _HTTP_RESOLVE_DEFAULT, NULL, NULL))
    {
      _cupsLangPrintf(stderr, _("%s: Unable to resolve \"%s\"."), "saveppd", uri);
      return (NULL);
    }

    uri = resolved;
  }

  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
  {
    _cupsLangPrintf(stderr, _("%s: Bad printer URI \"%s\"."), "saveppd", uri);
    return (NULL);
  }

  http = httpConnect2(host, port, NULL, AF_UNSPEC, !strcmp(scheme, "ipps") ? HTTP_ENCRYPTION_ALWAYS : HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);
  if (!http)
  {
    _cupsLangPrintf(stderr, _("%s: Unable to connect to \"%s:%d\": %s"), "saveppd", host, port, cupsLastErrorString());
    return (NULL);
  }

 /*
  * Send a Get-Printer-Attributes request...
  */

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  response = cupsDoRequest(http, request, resource);

  if (!_ppdCreateFromIPP(buffer, bufsize, response))
    _cupsLangPrintf(stderr, _("%s: Unable to create PPD file: %s"), "saveppd", strerror(errno));

  ippDelete(response);
  httpClose(http);

  if (buffer[0])
    return (buffer);
  else
    return (NULL);
}

static int                              /* O - 0 = success, -1 = error */
copy_file(const char *from,             /* I - Source file */
          const char *to               /* I - Destination file */
          )
{
  cups_file_t   *src,                   /* Source file */
                *dst;                   /* Destination file */
  int           bytes;                  /* Bytes to read/write */
  char          buffer[2048];           /* Copy buffer */



 /*
  * Open the source and destination file for a copy...
  */

  if ((src = cupsFileOpen(from, "rb")) == NULL)
    return (-1);

  if ((dst = cupsFileOpen(to, "a")) == NULL)
  {
    cupsFileClose(src);
    return (-1);
  }

 /*
  * Copy the source file to the destination...
  */

  while ((bytes = cupsFileRead(src, buffer, sizeof(buffer))) > 0)
    if (cupsFileWrite(dst, buffer, (size_t)bytes) < bytes)
    {
      cupsFileClose(src);
      cupsFileClose(dst);
      return (-1);
    }

 /*
  * Close both files and return...
  */

  cupsFileClose(src);

  return (cupsFileClose(dst));
}
