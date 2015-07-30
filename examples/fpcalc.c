#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>

#if defined(HAVE_SWRESAMPLE)
#include <libswresample/swresample.h>
#elif defined(HAVE_AVRESAMPLE)
#include <libavresample/avresample.h>
#endif

#include <chromaprint.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 28, 0)
#define avcodec_free_frame av_freep
#endif

// prh 2015-07-29
// for beefed up -version and new -md5 features

#include <config.h>
#include <libavutil/ffversion.h>
#define stringize2(aaa)  #aaa
#define stringize(aaa) stringize2(aaa)

#include <libavutil/md5.h>

void outputMD5(const char *var_name, struct AVMD5 *ctx)
	// finish up an md5 and fill in the supplied string
{
	int i;
	char md5_string[33];
	unsigned char digest[16];
	av_md5_final(ctx,digest);
	for (i=0; i<16; i++)
	{
		sprintf(&md5_string[i*2], "%02x", (unsigned int)digest[i]);
	}
	printf("%s=%s\n",var_name,md5_string);
}



int64_t get_default_channel_layout(int nb_channels)
{
/* 51.8.0 for FFmpeg, 51.26.0 for libav. I don't know how to identify them,
   so let's use the higher one. I really wish Ubuntu would stop being
   stupid and just drop libav. */
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51, 26, 0)
	return av_get_default_channel_layout(nb_channels);
#else
	switch (nb_channels) {
		case 1:
			return AV_CH_LAYOUT_MONO;
		case 2:
			return AV_CH_LAYOUT_STEREO;
		default:
			return 0;
	}
#endif
}

int decode_audio_file(ChromaprintContext *chromaprint_ctx, const char *file_name, int max_length, int *duration, int with_stream_md5)
{
	int ok = 0, remaining, length, consumed, codec_ctx_opened = 0, got_frame, stream_index;
	AVFormatContext *format_ctx = NULL;
	AVCodecContext *codec_ctx = NULL;
	AVCodec *codec = NULL;
	AVStream *stream = NULL;
	AVFrame *frame = NULL;
#if defined(HAVE_SWRESAMPLE)
	SwrContext *convert_ctx = NULL;
#elif defined(HAVE_AVRESAMPLE)
	AVAudioResampleContext *convert_ctx = NULL;
#else
	void *convert_ctx = NULL;
#endif
	int max_dst_nb_samples = 0, dst_linsize = 0;
	uint8_t *dst_data[1] = { NULL };
	uint8_t **data;
	AVPacket packet;

	struct AVMD5 *stream_md5_ctx = av_malloc(av_md5_size);
	av_md5_init(stream_md5_ctx);

	if (!strcmp(file_name, "-")) {
		file_name = "pipe:0";
	}

	if (avformat_open_input(&format_ctx, file_name, NULL, NULL) != 0) {
		fprintf(stderr, "ERROR: couldn't open the file\n");
		goto done;
	}

	if (avformat_find_stream_info(format_ctx, NULL) < 0) {
		fprintf(stderr, "ERROR: couldn't find stream information in the file\n");
		goto done;
	}

	stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
	if (stream_index < 0) {
		fprintf(stderr, "ERROR: couldn't find any audio stream in the file\n");
		goto done;
	}

	stream = format_ctx->streams[stream_index];

	codec_ctx = stream->codec;
	codec_ctx->request_sample_fmt = AV_SAMPLE_FMT_S16;

	if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
		fprintf(stderr, "ERROR: couldn't open the codec\n");
		goto done;
	}
	codec_ctx_opened = 1;

	if (codec_ctx->channels <= 0) {
		fprintf(stderr, "ERROR: no channels found in the audio stream\n");
		goto done;
	}

	if (codec_ctx->sample_fmt != AV_SAMPLE_FMT_S16) {
		int64_t channel_layout = codec_ctx->channel_layout;
		if (!channel_layout) {
			channel_layout = get_default_channel_layout(codec_ctx->channels);
		}
#if defined(HAVE_SWRESAMPLE)
		convert_ctx = swr_alloc_set_opts(NULL,
			channel_layout, AV_SAMPLE_FMT_S16, codec_ctx->sample_rate,
			channel_layout, codec_ctx->sample_fmt, codec_ctx->sample_rate,
			0, NULL);
		if (!convert_ctx) {
			fprintf(stderr, "ERROR: couldn't allocate audio converter\n");
			goto done;
		}
		if (swr_init(convert_ctx) < 0) {
			fprintf(stderr, "ERROR: couldn't initialize the audio converter\n");
			goto done;
		}
#elif defined(HAVE_AVRESAMPLE)
		convert_ctx = avresample_alloc_context();
		av_opt_set_int(convert_ctx, "out_channel_layout", channel_layout, 0);
		av_opt_set_int(convert_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
		av_opt_set_int(convert_ctx, "out_sample_rate", codec_ctx->sample_rate, 0);
		av_opt_set_int(convert_ctx, "in_channel_layout", channel_layout, 0);
		av_opt_set_int(convert_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0);
		av_opt_set_int(convert_ctx, "in_sample_rate", codec_ctx->sample_rate, 0);
		if (!convert_ctx) {
			fprintf(stderr, "ERROR: couldn't allocate audio converter\n");
			goto done;
		}
		if (avresample_open(convert_ctx) < 0) {
			fprintf(stderr, "ERROR: couldn't initialize the audio converter\n");
			goto done;
		}
#else
		fprintf(stderr, "ERROR: unsupported audio format (please build fpcalc with libswresample)\n");
		goto done;
#endif
	}

	if (stream->duration != AV_NOPTS_VALUE) {
		*duration = stream->time_base.num * stream->duration / stream->time_base.den;
	}
	else if (format_ctx->duration != AV_NOPTS_VALUE) {
		*duration = format_ctx->duration / AV_TIME_BASE;
	}
	else {
		fprintf(stderr, "ERROR: couldn't detect the audio duration\n");
		goto done;
	}

	remaining = max_length * codec_ctx->channels * codec_ctx->sample_rate;
	chromaprint_start(chromaprint_ctx, codec_ctx->sample_rate, codec_ctx->channels);

	frame = avcodec_alloc_frame();

	while (1) {
		if (av_read_frame(format_ctx, &packet) < 0) {
			break;
		}

		if (packet.stream_index == stream_index) {
			avcodec_get_frame_defaults(frame);

			got_frame = 0;
			consumed = avcodec_decode_audio4(codec_ctx, frame, &got_frame, &packet);
			if (consumed < 0) {
				fprintf(stderr, "WARNING: error decoding audio\n");
				continue;
			}

			if (got_frame) {

				if (with_stream_md5)
					av_md5_update(stream_md5_ctx, packet.data, packet.size);

				// continue getting packets, but do not continue
				// converting and fingerprinting if there's no more "remaining",
				// that means we are doing with_stream_md5 past max_length

				if (max_length && remaining <= 0)
				{
					av_free_packet(&packet);
					continue;
				}

				data = frame->data;
				if (convert_ctx) {
					if (frame->nb_samples > max_dst_nb_samples) {
						av_freep(&dst_data[0]);
						if (av_samples_alloc(dst_data, &dst_linsize, codec_ctx->channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1) < 0) {
							fprintf(stderr, "ERROR: couldn't allocate audio converter buffer\n");
							goto done;
						}
						max_dst_nb_samples = frame->nb_samples;
					}
#if defined(HAVE_SWRESAMPLE)
					if (swr_convert(convert_ctx, dst_data, frame->nb_samples, (const uint8_t **)frame->data, frame->nb_samples) < 0)
#elif defined(HAVE_AVRESAMPLE)
					if (avresample_convert(convert_ctx, dst_data, 0, frame->nb_samples, (uint8_t **)frame->data, 0, frame->nb_samples) < 0)
#endif
					{
						fprintf(stderr, "ERROR: couldn't convert the audio\n");
						goto done;
					}
					data = dst_data;
				}
				length = MIN(remaining, frame->nb_samples * codec_ctx->channels);
				if (!chromaprint_feed(chromaprint_ctx, data[0], length)) {
					goto done;
				}

				if (max_length) {
					remaining -= length;

					// continue looping over whole stream if
					// getting it's stream_md5

					if (!with_stream_md5 && remaining <= 0) {
						goto finish;
					}
				}
			}
		}
		av_free_packet(&packet);
	}

finish:
	if (!chromaprint_finish(chromaprint_ctx)) {
		fprintf(stderr, "ERROR: fingerprint calculation failed\n");
		goto done;
	}

	ok = 1;

done:

	av_free_packet(&packet);
		// was missing on jumps out of loop

	if (frame) {
		avcodec_free_frame(&frame);
	}
	if (dst_data[0]) {
		av_freep(&dst_data[0]);
	}
	if (convert_ctx) {
#if defined(HAVE_SWRESAMPLE)
		swr_free(&convert_ctx);
#elif defined(HAVE_AVRESAMPLE)
		avresample_free(&convert_ctx);
#endif
	}
	if (codec_ctx_opened) {
		avcodec_close(codec_ctx);
	}
	if (format_ctx) {
		avformat_close_input(&format_ctx);
	}

	if (ok && with_stream_md5)
		outputMD5("STREAM_MD5",stream_md5_ctx);
	if (stream_md5_ctx)
		av_free(stream_md5_ctx);

	return ok;
}

int fpcalc_main(int argc, char **argv)
{
	int i, j, max_length = 120, num_file_names = 0, raw = 0, raw_fingerprint_size, duration;
	int32_t *raw_fingerprint;
	char *file_name, *fingerprint, **file_names;
	ChromaprintContext *chromaprint_ctx;
	int algo = CHROMAPRINT_ALGORITHM_DEFAULT, num_failed = 0, do_hash = 0;

	int with_stream_md5 = 0;
	int with_fingerprint_md5 = 0;
	int with_fingerprint_ints = 0;
	int av_log_level = AV_LOG_ERROR;

	file_names = malloc(argc * sizeof(char *));
	for (i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-length") && i + 1 < argc) {
			max_length = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "-version") || !strcmp(arg, "-v")) {
			printf("fpcalc version %s\n", chromaprint_get_version());
			printf("ffmpeg_version=%s\n", stringize(FFMPEG_VERSION));
				// ffmpeg_version should be from a major release
			printf("libavutil_version=%s\n",stringize(LIBAVUTIL_VERSION));
			printf("libavcodec_version=%s\n",stringize(LIBAVCODEC_VERSION));
			printf("libavformat_version=%s\n",stringize(LIBAVFORMAT_VERSION));
			#if defined(HAVE_SWRESAMPLE)
				#define LIBSWRESAMPLE_VERSION  \
					AV_VERSION(LIBSWRESAMPLE_VERSION_MAJOR,   \
							   LIBSWRESAMPLE_VERSION_MINOR,   \
							   LIBSWRESAMPLE_VERSION_MICRO)
				 printf("libswresample_version=%s\n",stringize(LIBSWRESAMPLE_VERSION));
			#elif defined(HAVE_AVRESAMPLE)
				printf("libavresample_version=%s\n",stringize(LIBAVRESAMPLE_VERSION));
			#endif
			printf("ffmpeg_configuration=%s\n",stringize(FFMPEG_CONFIGURATION));

			return 0;
		}
		else if (!strcmp(arg, "-raw")) {
			raw = 1;
		}
		else if (!strcmp(arg, "-hash")) {
			do_hash = 1;
		}
		else if (!strcmp(arg, "-algo") && i + 1 < argc) {
			const char *v = argv[++i];
			if (!strcmp(v, "test1")) { algo = CHROMAPRINT_ALGORITHM_TEST1; }
			else if (!strcmp(v, "test2")) { algo = CHROMAPRINT_ALGORITHM_TEST2; }
			else if (!strcmp(v, "test3")) { algo = CHROMAPRINT_ALGORITHM_TEST3; }
			else if (!strcmp(v, "test4")) { algo = CHROMAPRINT_ALGORITHM_TEST4; }
			else {
				fprintf(stderr, "WARNING: unknown algorithm, using the default\n");
			}
		}

		else if (!strcmp(arg, "-md5"))
		{
			with_fingerprint_md5 = 1;
		}
		else if (!strcmp(arg, "-stream_md5"))
		{
			with_stream_md5 = 1;
		}
		else if (!strcmp(arg, "-ints"))
		{
			with_fingerprint_ints = 1;
		}
		else if (!strcmp(arg, "-av_log") && i + 1 < argc)
		{
			av_log_level = atoi(argv[++i]);
		}

		else if (!strcmp(arg, "-set") && i + 1 < argc) {
			i += 1;
		}
		else {
			file_names[num_file_names++] = argv[i];
		}
	}

	if (!num_file_names) {
		printf("\n");
		printf("usage: %s [OPTIONS] FILE...\n\n", argv[0]);
		printf("Options:\n");
		printf("  -version      print version information\n");
		printf("  -length SECS  length of the audio data used for fingerprint calculation (default 120)\n");
		printf("  -raw          output the raw uncompressed fingerprint\n");
		printf("  -algo NAME    version of the fingerprint algorithm\n");
		printf("  -hash         calculate also the fingerprint hash\n");
		printf("  -md5          calculate the md5 hash of the fingerprint (not with -raw)\n");
		printf("  -stream_md5   calculate the md5 hash of the entire stream, which should agree across platforms for the same audio file\n");
		printf("  -ints         same output as RAW but called FINGERPRINT_INTS in addition to the text fingerprint\n");
		printf("  -av_log NUM   set the av_log_level (default=AV_LOG_ERROR=16)\n");
		printf("\n");
		printf("  -set silence_threshold=THRESHOLD\n");
		printf("\n");
		return 2;
	}

	av_register_all();
	av_log_set_level(av_log_level);

	chromaprint_ctx = chromaprint_new(algo);

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-set") && i + 1 < argc) {
			char *name = argv[++i];
			char *value = strchr(name, '=');
			if (value) {
				*value++ = '\0';
				chromaprint_set_option(chromaprint_ctx, name, atoi(value));
			}
		}
	}

	for (i = 0; i < num_file_names; i++) {
		file_name = file_names[i];
		if (!decode_audio_file(chromaprint_ctx, file_name, max_length, &duration, with_stream_md5)) {
			fprintf(stderr, "ERROR: unable to calculate fingerprint for file %s, skipping\n", file_name);
			num_failed++;
			continue;
		}
		if (i > 0) {
			printf("\n");
		}
		printf("FILE=%s\n", file_name);
		printf("DURATION=%d\n", duration);
		if (raw || with_fingerprint_ints) {
			if (!chromaprint_get_raw_fingerprint(chromaprint_ctx, (void **)&raw_fingerprint, &raw_fingerprint_size)) {
				fprintf(stderr, "ERROR: unable to calculate fingerprint for file %s, skipping\n", file_name);
				num_failed++;
				continue;
			}
			printf(with_fingerprint_ints?"FINGERPRINT_INTS=":"FINGERPRINT=");
			for (j = 0; j < raw_fingerprint_size; j++) {
				printf("%d%s", raw_fingerprint[j], j + 1 < raw_fingerprint_size ? "," : "");
			}
			printf("\n");
			chromaprint_dealloc(raw_fingerprint);
		}
		if (!raw) {
			if (!chromaprint_get_fingerprint(chromaprint_ctx, &fingerprint)) {
				fprintf(stderr, "ERROR: unable to calculate fingerprint for file %s, skipping\n", file_name);
				num_failed++;
				continue;
			}
			printf("FINGERPRINT=%s\n", fingerprint);
			if (with_fingerprint_md5)
			{
				struct AVMD5 *fp_ctx = av_malloc(av_md5_size);
				av_md5_init(fp_ctx);
				av_md5_update(fp_ctx, fingerprint, strlen(fingerprint));
				outputMD5("FINGERPRINT_MD5",fp_ctx);
				av_free(fp_ctx);
			}
			chromaprint_dealloc(fingerprint);
		}
        if (do_hash) {
            int32_t hash = 0;
            chromaprint_get_fingerprint_hash(chromaprint_ctx, &hash);
            printf("HASH=%d\n", hash);
        }
	}

	chromaprint_free(chromaprint_ctx);
	free(file_names);

	return num_failed ? 1 : 0;
}

#ifdef _WIN32
int main(int win32_argc, char **win32_argv)
{
	int i, argc = 0, buffsize = 0, offset = 0;
	char **utf8_argv, *utf8_argv_ptr;
	wchar_t **argv;

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	buffsize = 0;
	for (i = 0; i < argc; i++) {
		buffsize += WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, NULL, 0, NULL, NULL);
	}

	utf8_argv = av_mallocz(sizeof(char *) * (argc + 1) + buffsize);
	utf8_argv_ptr = (char *)utf8_argv + sizeof(char *) * (argc + 1);

	for (i = 0; i < argc; i++) {
		utf8_argv[i] = &utf8_argv_ptr[offset];
		offset += WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, &utf8_argv_ptr[offset], buffsize - offset, NULL, NULL);
	}

	LocalFree(argv);

	return fpcalc_main(argc, utf8_argv);
}
#else
int main(int argc, char **argv)
{
	return fpcalc_main(argc, argv);
}
#endif
