# Commands

OpenLTFS consists of a client interface and a set of background processes
that perform the processing of requests. Beside the special case of
transparent recalls requests that are driven by file i/o all other
operations are initiated by the client interface.

The following client interface command exist:
<pre>
    commands:
          @subpage ltfsdm_help          "ltfsdm help"              - gives an overview
          @subpage ltfsdm_start         "ltfsdm start"             - start the Open LTFS service in background
          @subpage ltfsdm_stop          "ltfsdm stop"              - stop the Open LTFS service
          @subpage ltfsdm_add           "ltfsdm add"               - adds Open LTFS management to a file system
          @subpage ltfsdm_status        "ltfsdm status"            - provides information if the back end has been started
          @subpage ltfsdm_migrate       "ltfsdm migrate"           - migrate file system objects from the local file system to tape
          @subpage ltfsdm_recall        "ltfsdm recall"            - recall file system objects back from tape to local disk
          @subpage ltfsdm_retrieve      "ltfsdm retrieve"          - synchronizes the inventory with the information provided by Spectrum Archive LE
          @subpage ltfsdm_version       "ltfsdm version"           - provides the version number of Open LTFS
    info sub commands:
          @subpage ltfsdm_info_requests "ltfsdm info requests"     - retrieve information about all or a specific Open LTFS requests
          @subpage ltfsdm_info_jobs     "ltfsdm info jobs"         - retrieve information about all or a specific Open LTFS jobs
          @subpage ltfsdm_info_files    "ltfsdm info files"        - retrieve information about the migration state of file system objects
          @subpage ltfsdm_info_fs       "ltfsdm info fs"           - lists the file systems managed by Open LTFS
          @subpage ltfsdm_info_drives   "ltfsdm info drives"       - lists the drives known to OpenLTFS
          @subpage ltfsdm_info_tapes    "ltfsdm info tapes"        - lists the cartridges known to OpenLTFS
          @subpage ltfsdm_info_pools    "ltfsdm info pools"        - lists all defined tape storage pools and their sizes
    pool sub commands:
          @subpage ltfsdm_pool_create   "ltfsdm pool create"       - create a tape storage pool
          @subpage ltfsdm_pool_delete   "ltfsdm pool delete"       - delete a tape storage pool
          @subpage ltfsdm_pool_add      "ltfsdm pool add"          - add a cartridge to a tape storage pool
          @subpage ltfsdm_pool_remove   "ltfsdm pool remove"       - removes a cartridge from a tape storage pool
</pre>

To start OpenLTFS the @ref ltfsdm_start "ltfsdm start" is to be used:

@verbatim
	[root@visp ~]# ltfsdm start
	LTFSDMC0099I(0073): Starting the OpenLTFS backend service.
	LTFSDMX0029I(0062): OpenLTFS version: 0.0.624-master.2017-11-09T10.57.51
	LTFSDMC0100I(0097): Connecting.
	LTFSDMC0097I(0141): The OpenLTFS server process has been started with pid  27905.
@endverbatim
 