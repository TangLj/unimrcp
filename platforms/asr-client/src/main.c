#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <apr_getopt.h>
#include <apr_file_info.h>
#include <apr_thread_proc.h>
#include <apr_time.h>
#include "asr_engine.h"
#include "asr_engine_common.h"
#include "apt_dir_layout.h"

#define DEFAULT_GRAMMAR_FILE "grammar.xml"
#define DEFAULT_PARAMS_FILE "params_default.txt"
#define DEFAULT_AUDIO_FILE "one-8kHz.wav"
#define DEFAULT_AUDIO_DIR "audio"
#define DEFAULT_PROFILE "uni2"
#define DEFAULT_TYPE 0
#define DEFAULT_CONCURRENT_NUMBER 1
#define DEFAULT_INTERVAL_TIME 500

#define DEFAULT_NUM "1"
#define DEFAULT_CLIENT_PORT "8062"
#define DEFAULT_CLIENT_RTP_MIN "4000"
#define DEFAULT_CLIENT_RTP_MAX "5000"
#define DEFAULT_SERVER_IP "auto"
#define DEFAULT_SERVER_PORT "8060"

typedef struct {
	const char        *root_dir_path;
	apt_log_priority_e log_priority;
	apt_log_output_e   log_output;
	apr_pool_t        *pool;
	apr_int16_t        type;
	const char        *audio_file_path;
	const char        *audio_dir_path;
	apr_int16_t        conc;
	apr_int16_t        inter;
	apr_int16_t        total;
	const char        *extra;
	const char        *xmlconfig;
} client_options_t;

typedef struct
{
	apr_int16_t         m_total;
	apr_int16_t         m_conc;
} asr_context_t;

typedef struct {
	asr_engine_t      *engine;
	const char        *grammar_file;
	const char        *input_file;
	const char        *profile;
	const char        *params_file;
	apr_thread_t      *thread;
	asr_context_t     *context;
	apr_int16_t        sequence;
} asr_params_t;

/** Thread function to run ASR scenario in */
static void* APR_THREAD_FUNC asr_session_run(apr_thread_t *thread, void *data)
{
	const char *result;
	asr_params_t *params = data;
	asr_session_t *session = asr_session_create(params->engine,params->profile);
	if(session) {
		// if(params->send_set_params) {
		// 	// Set parameters from param file
		// 	asr_session_set_param(session,params->params_file,NULL,NULL);
		// }
		// Do recognition
		result = asr_session_file_recognize(session,params->grammar_file,params->input_file,params->params_file,TRUE);
		apr_time_exp_t tmp_ptr;
		apr_time_t now = apr_time_now();
		apr_time_exp_lt(&tmp_ptr, now);
		printf("%4d-%02d-%02d %02d:%02d:%02d:%06d [ASRCLIENT] [resulted] [seq-%d] [file:%s] [text:%s]\n",
			1900 + tmp_ptr.tm_year, 1 + tmp_ptr.tm_mon, tmp_ptr.tm_mday,
			tmp_ptr.tm_hour, tmp_ptr.tm_min, tmp_ptr.tm_sec, tmp_ptr.tm_usec,
			params->sequence, params->input_file, result
		);
		asr_session_destroy(session);
	}
	params->context->m_conc -= 1;
	apr_time_exp_t tmp_ptr;
	apr_time_t now = apr_time_now();
	apr_time_exp_lt(&tmp_ptr, now);
	printf("%4d-%02d-%02d %02d:%02d:%02d:%06d [ASRCLIENT] [finished] [%d] [%d]\n",
		1900 + tmp_ptr.tm_year, 1 + tmp_ptr.tm_mon, tmp_ptr.tm_mday,
		tmp_ptr.tm_hour, tmp_ptr.tm_min, tmp_ptr.tm_sec, tmp_ptr.tm_usec,
		params->sequence, params->context->m_conc
	);
	return NULL;
}

/** Launch demo ASR session */
static apt_bool_t asr_session_launch(asr_engine_t *engine, const char *input_file, const char* params_file)
{
	apr_pool_t *pool;
	apr_pool_create(&pool,NULL);
	asr_context_t *context;
	context = apr_palloc(pool,sizeof(asr_context_t));
	context->m_total = 0;
	context->m_conc = 0;
	asr_params_t *params;
	params = apr_palloc(pool,sizeof(asr_params_t));
	params->context = context;
	params->engine = engine;
	params->grammar_file = DEFAULT_GRAMMAR_FILE;
	params->profile = DEFAULT_PROFILE;
	params->input_file = apr_pstrdup(pool,input_file);
	if(params_file && params_file[0] != '-') {
		params->params_file = apr_pstrdup(pool,params_file);
	}
	else {
		params->params_file = NULL;
	}
	/* Launch a thread to run demo ASR session in */
	if(apr_thread_create(&params->thread,NULL,asr_session_run,params,pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return FALSE;
	}else{
		context->m_conc += 1;
		context->m_total += 1;
		params->sequence = context->m_total;
		apr_time_exp_t tmp_ptr;
		apr_time_t now = apr_time_now();
		apr_time_exp_lt(&tmp_ptr, now);
		printf("%4d-%02d-%02d %02d:%02d:%02d:%06d [ASRCLIENT] [launched] [%d] [%d] [file:%s]\n",
			1900 + tmp_ptr.tm_year, 1 + tmp_ptr.tm_mon, tmp_ptr.tm_mday,
			tmp_ptr.tm_hour, tmp_ptr.tm_min, tmp_ptr.tm_sec, tmp_ptr.tm_usec,
			context->m_total, context->m_conc,
			params->input_file
		);
	}
	// 每2s检查一下是否完成识别
	do
	{
		usleep(2000* 1000);
		apr_time_exp_t tmp_ptr;
		apr_time_t now = apr_time_now();
		apr_time_exp_lt(&tmp_ptr, now);
		printf("%4d-%02d-%02d %02d:%02d:%02d:%06d [ASRCLIENT] [awaiting] [%d] [%d]\n",
			1900 + tmp_ptr.tm_year, 1 + tmp_ptr.tm_mon, tmp_ptr.tm_mday,
			tmp_ptr.tm_hour, tmp_ptr.tm_min, tmp_ptr.tm_sec, tmp_ptr.tm_usec,
			context->m_total, context->m_conc
		);
	} while (context->m_conc > 0);
	apr_pool_destroy(pool);
	return TRUE;
}

/** Launch demo ASR session batch */
static apt_bool_t asr_session_launch_batch(asr_engine_t *engine, const char *input_file, const char* params_file, int conc, int inter, int total)
{
	apr_pool_t *pool;
	apr_pool_create(&pool,NULL);
	asr_context_t *context;
	context = apr_palloc(pool,sizeof(asr_context_t));
	context->m_total = 0;
	context->m_conc = 0;
	const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(engine->mrcp_app);
	const char *audio_dir_path = apt_datadir_filepath_get(dir_layout,input_file,pool);
	DIR *dir;
	struct dirent *ptr;
	if((dir=opendir(audio_dir_path)) == NULL){
		printf("************************************Cant open audio dir.\n");
		return TRUE;
	}
	if (total == 0) {
		while((ptr=readdir(dir)) != NULL){
			char *ext = ptr->d_name + strlen(ptr->d_name) - 3;
			if(!strcmp(ext, "wav")){
				total += 1;
			}
		}
		rewinddir(dir);
	}
	// 每(inter)ms检查是否需要开启一个新的识别线程
	do {
		usleep(inter * 1000);
		apr_time_exp_t tmp_ptr;
		apr_time_t now = apr_time_now();
		apr_time_exp_lt(&tmp_ptr, now);
		if (context->m_conc < conc) {
			asr_params_t *params;
			params = apr_palloc(pool,sizeof(asr_params_t));
			params->context = context;
			params->engine = engine;
			params->grammar_file = DEFAULT_GRAMMAR_FILE;
			params->profile = DEFAULT_PROFILE;
			
			char audio_name[200];
			apt_bool_t is_continue = FALSE;
			if ((ptr=readdir(dir)) == NULL) {
				rewinddir(dir);
				ptr=readdir(dir);
			}
			do
			{
				char *ext = ptr->d_name + strlen(ptr->d_name) - 3;
				if(!strcmp(ext, "wav")){
					memset(audio_name, '\0', sizeof(audio_name));
					strcpy(audio_name, audio_dir_path);
					strcat(audio_name, "/");
					strcat(audio_name, ptr->d_name);
					params->input_file = apr_pstrdup(pool,audio_name);
					break;
				}
			} while ((ptr=readdir(dir)) != NULL);
			if(params_file && params_file[0] != '-') {
				params->params_file = apr_pstrdup(pool,params_file);
			}
			else {
				params->params_file = NULL;
			}
			/* Launch a thread to run demo ASR session in */
			if(apr_thread_create(&params->thread,NULL,asr_session_run,params,pool) != APR_SUCCESS) {
				apr_pool_destroy(pool);
				return FALSE;
			}else{
				context->m_conc += 1;
				context->m_total += 1;
				params->sequence = context->m_total;
				printf("%4d-%02d-%02d %02d:%02d:%02d:%06d [ASRCLIENT] [launched] [%d] [%d] [file:%s]\n",
					1900 + tmp_ptr.tm_year, 1 + tmp_ptr.tm_mon, tmp_ptr.tm_mday,
					tmp_ptr.tm_hour, tmp_ptr.tm_min, tmp_ptr.tm_sec, tmp_ptr.tm_usec,
					context->m_total, context->m_conc,
					params->input_file
				);
			}
		}else{
			printf("%4d-%02d-%02d %02d:%02d:%02d:%06d [ASRCLIENT] [awaiting] [%d] [%d]\n",
				1900 + tmp_ptr.tm_year, 1 + tmp_ptr.tm_mon, tmp_ptr.tm_mday,
				tmp_ptr.tm_hour, tmp_ptr.tm_min, tmp_ptr.tm_sec, tmp_ptr.tm_usec,
				context->m_total, context->m_conc
			);
		}
	} while (context->m_total < total);
	closedir(dir);
	// 每0.5s检查一下是否完成识别
	do {
		usleep(500* 1000);
		apr_time_exp_t tmp_ptr;
		apr_time_t now = apr_time_now();
		apr_time_exp_lt(&tmp_ptr, now);
		printf("%4d-%02d-%02d %02d:%02d:%02d:%06d [ASRCLIENT] [awaiting] [%d] [%d]\n",
			1900 + tmp_ptr.tm_year, 1 + tmp_ptr.tm_mon, tmp_ptr.tm_mday,
			tmp_ptr.tm_hour, tmp_ptr.tm_min, tmp_ptr.tm_sec, tmp_ptr.tm_usec,
			context->m_total, context->m_conc
		);
	} while (context->m_conc > 0);
	apr_pool_destroy(pool);
	return TRUE;
}

static void usage(void)
{
	printf(
		"\n"
		"用法:\n"
		"\n"
		"  asrclient [选项]\n"
		"\n"
		"  可用选项:\n"
		"\n"
		"   -r [--root-dir] path     : 设置项目根目录。\n"
		"\n"
		"   -l [--log-prio] priority : 设置日志等级。\n"
		"                              (0-emergency, ..., 7-debug, 默认7)\n"
		"\n"
		"   -o [--log-output] mode   : 设置日志输出模式。\n"
		"                              (0-无, 1-仅控制台, 2-仅文件, 3-控制台和文件, 默认1)\n"
		"\n"
		"   -t [--type] number       : 测试类型。\n"
		"                              (0-单音频, 1-批量音频, 默认0。)\n"
		"\n"
		"   -f [--audio-file] path   : 测试音频路径(type为0时生效), 默认'one-8kHz.wav'。\n"
		"\n"
		"   -d [--audio-dir] path    : 测试音频路径(type为1时生效), 默认'audio'。\n"
		"\n"
		"   -c [--conc] number       : 设置测试并发数(type为1时生效), 默认1。\n"
		"\n"
		"   -i [--interval] time     : 设置并发测试发起间隔时间(type为1时生效), 默认为500(ms)。\n"
		"\n"
		"   -a [--total] number      : 设置测试总次数(type为1时生效), 默认为音频总数。\n"
		"\n"
		"   -h [--help]              : 显示使用方法。\n"
		"\n");
}

static void options_destroy(client_options_t *options)
{
	if(options->pool) {
		apr_pool_destroy(options->pool);
	}
}

static client_options_t* options_load(int argc, const char * const *argv)
{
	apr_status_t rv;
	apr_getopt_t *opt = NULL;
	int optch;
	const char *optarg;
	apr_pool_t *pool;
	client_options_t *options;

	const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "root-dir",    'r', TRUE,  "path to root dir" },  /* -r arg or --root-dir arg */
		{ "log-prio",    'l', TRUE,  "log priority" },      /* -l arg or --log-prio arg */
		{ "log-output",  'o', TRUE,  "log output mode" },   /* -o arg or --log-output arg */
		{ "type",        't', TRUE,  "test type" },
		{ "audio-file",  'f', TRUE,  "path to audio file" },
		{ "audio-dir",   'd', TRUE,  "path to audio dir" },
		{ "conc",        'c', TRUE,  "concurrent number" },
		{ "interval",    'i', TRUE,  "interval time" },
		{ "total",       'a', TRUE,  "total number" },
		{ "extra",       'e', TRUE,  "extra params" },
		{ "help",        'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	/* create APR pool to allocate options from */
	apr_pool_create(&pool,NULL);
	if(!pool) {
		return NULL;
	}
	options = apr_palloc(pool,sizeof(client_options_t));
	options->pool = pool;
	/* set the default options */
	options->root_dir_path = NULL;
	options->log_priority = APT_PRIO_DEBUG;
	options->log_output = APT_LOG_OUTPUT_FILE;
	options->type = DEFAULT_TYPE;
	options->audio_file_path = DEFAULT_AUDIO_FILE;
	options->audio_dir_path = DEFAULT_AUDIO_DIR;
	options->conc = DEFAULT_CONCURRENT_NUMBER;
	options->inter = DEFAULT_INTERVAL_TIME;
	options->total = 0;
	options->extra = NULL;
	options->xmlconfig = NULL;
	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv != APR_SUCCESS) {
		options_destroy(options);
		return NULL;
	}

	while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
		switch(optch) {
			case 'r':
				options->root_dir_path = optarg;
				break;
			case 'l':
				if(optarg) {
					options->log_priority = atoi(optarg);
				}
				break;
			case 'o':
				if(optarg) {
					options->log_output = atoi(optarg);
				}
				break;
			case 't':
				if (optarg) {
					options->type = atoi(optarg);
				}
				break;
			case 'f':
				options->audio_file_path = optarg;
				break;
			case 'd':
				options->audio_dir_path = optarg;
				break;
			case 'c':
				if (optarg) {
					options->conc = atoi(optarg);
				}
				break;
			case 'i':
				if (optarg) {
					options->inter = atoi(optarg);
				}
				break;
			case 'a':
				if (optarg) {
					options->total = atoi(optarg);
				}
				break;
			case 'e':
				options->extra = optarg;
				break;
			case 'h':
				usage();
				return FALSE;
		}
	}
	if (options->type == 1 && options->total != 0 && options->conc > options->total) {
		printf("************************************Concurrent number(%d) is greater then total number(%d)，reset as the same value.\n", options->conc, options->total);
		options->conc = options->total;
	}

	if(rv != APR_EOF) {
		usage();
		options_destroy(options);
		return NULL;
	}

	return options;
}

int main(int argc, const char * const *argv)
{
	client_options_t *options;
	asr_engine_t *engine;

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	/* load options */
	options = options_load(argc,argv);
	if(!options) {
		apr_terminate();
		return 0;
	}

	apr_pool_t *pool = NULL;
	apr_pool_create(&pool,NULL);
	if(!pool) {
		return 0;
	}
	if(options->extra) {
		char *n = DEFAULT_NUM, *client_port = DEFAULT_CLIENT_PORT, *client_rtp_port_min = DEFAULT_CLIENT_RTP_MIN, *client_rtp_port_max = DEFAULT_CLIENT_RTP_MAX, *server_ip = DEFAULT_SERVER_IP, *server_port = DEFAULT_SERVER_PORT;
		char *key, *value, *strtok_state;
		char *extra = apr_pstrdup(pool, options->extra);
		key = apr_strtok(extra, ",", &strtok_state);
		while (key) {
			value = strchr(key, '=');
			if (value) {
				*value = '\0';      /* Split the string in two */
				value++;            /* Skip passed the = */
			}
			if(apr_strnatcmp(key, "n") == 0) {
				n = value;
			}
			if(apr_strnatcmp(key, "client_port") == 0) {
				client_port = value;
			}
			if(apr_strnatcmp(key, "client_rtp_port_min") == 0) {
				client_rtp_port_min = value;
			}
			if(apr_strnatcmp(key, "client_rtp_port_max") == 0) {
				client_rtp_port_max = value;
			}
			if(apr_strnatcmp(key, "server_ip") == 0) {
				server_ip = value;
			}
			if(apr_strnatcmp(key, "server_port") == 0) {
				server_port = value;
			}
			
			key = apr_strtok(NULL, ",", &strtok_state);
		}
		options->xmlconfig = apr_psprintf(pool, "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \
			<unimrcpclient xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" \
										xsi:noNamespaceSchemaLocation=\"unimrcpclient.xsd\" \
										version=\"1.0\"\
										subfolder=\"client-profiles\">\
				<properties>\
					<ip type=\"auto\"/>\
				</properties>\
				<components>\
					<resource-factory>\
						<resource id=\"speechsynth\" enable=\"true\"/>\
						<resource id=\"speechrecog\" enable=\"true\"/>\
						<resource id=\"recorder\" enable=\"true\"/>\
						<resource id=\"speakverify\" enable=\"true\"/>\
					</resource-factory>\
					<sip-uac id=\"SIP-Agent-%s\" type=\"SofiaSIP\">\
						<sip-port>%s</sip-port>\
						<sip-transport>udp</sip-transport>\
						<ua-name>UniMRCP SofiaSIP</ua-name>\
						<sdp-origin>UniMRCPClient</sdp-origin>\
					</sip-uac>\
					<rtsp-uac id=\"RTSP-Agent-%s\" type=\"UniRTSP\">\
						<max-connection-count>100</max-connection-count>\
						<sdp-origin>UniMRCPClient</sdp-origin>\
					</rtsp-uac>\
					<mrcpv2-uac id=\"MRCPv2-Agent-%s\">\
						<max-connection-count>100</max-connection-count>\
						<max-shared-use-count>100</max-shared-use-count>\
						<offer-new-connection>false</offer-new-connection>\
						<rx-buffer-size>1024</rx-buffer-size>\
						<tx-buffer-size>1024</tx-buffer-size>\
					</mrcpv2-uac>\
					<media-engine id=\"Media-Engine-%s\">\
						<realtime-rate>1</realtime-rate>\
					</media-engine>\
					<rtp-factory id=\"RTP-Factory-%s\">\
						<rtp-port-min>%s</rtp-port-min>\
						<rtp-port-max>%s</rtp-port-max>\
					</rtp-factory>\
				</components>\
				<settings>\
					<rtp-settings id=\"RTP-Settings-%s\">\
						<jitter-buffer>\
							<adaptive>1</adaptive>\
							<playout-delay>50</playout-delay>\
							<max-playout-delay>600</max-playout-delay>\
							<time-skew-detection>1</time-skew-detection>\
						</jitter-buffer>\
						<ptime>20</ptime>\
						<codecs>PCMU PCMA L16/96/8000 telephone-event/101/8000</codecs>\
						<rtcp enable=\"false\">\
							<rtcp-bye>1</rtcp-bye>\
							<tx-interval>5000</tx-interval>\
							<rx-resolution>1000</rx-resolution>\
						</rtcp>\
					</rtp-settings>\
					<sip-settings id=\"UniMRCP-SIP-Settings\">\
						<server-ip>%s</server-ip>\
						<server-port>%s</server-port>\
					</sip-settings>\
				</settings>\
				<profiles>\
					<mrcpv2-profile id=\"uni2\">\
						<sip-uac>SIP-Agent-%s</sip-uac>\
						<mrcpv2-uac>MRCPv2-Agent-%s</mrcpv2-uac>\
						<media-engine>Media-Engine-%s</media-engine>\
						<rtp-factory>RTP-Factory-%s</rtp-factory>\
						<sip-settings>UniMRCP-SIP-Settings</sip-settings>\
						<rtp-settings>RTP-Settings-%s</rtp-settings>\
					</mrcpv2-profile>\
				</profiles>\
			</unimrcpclient>", n, client_port, n, n, n, n, client_rtp_port_min, client_rtp_port_max, n, server_ip, server_port, n, n, n, n, n);
	}
	/* create asr engine */
	engine = asr_engine_create3(
				options->root_dir_path,
				options->xmlconfig,
				options->log_priority,
				options->log_output);
	apr_pool_destroy(pool);
	if(engine) {
		/* run command line  */
		if (options->type == 0)
		{
			printf("************************************Test type: 0, audio file path: %s.\n", options->audio_file_path);
			asr_session_launch(engine, options->audio_file_path, NULL);
		}else if (options->type == 1)
		{
			printf("************************************Test type: 1, audio dir path: %s, concurrent number: %d, interval time: %d(ms), total number: %d.\n", options->audio_dir_path, options->conc, options->inter, options->total);
			asr_session_launch_batch(engine, options->audio_dir_path, NULL, options->conc, options->inter, options->total);
		}else {
			printf("************************************Test type: %d, nothing to do.\n", options->type);
		}
		/* destroy demo framework */
		asr_engine_destroy(engine);
	}

	/* destroy options */
	options_destroy(options);

	/* APR global termination */
	apr_terminate();
	return 0;
}
