#include "source\wav_source.h"
#include "filters\dvd_graph.h"
#include "sink\sink_dsound.h"

int main(int argc, char *argv[])
{
	const char	*input_filename = "f:\\test.wav";

	bool		print_process = true;
	DVDGraph	dvd_graph;
	DSoundSink	dsound;
	Sink		*sink = &dsound;
	Chunk		chunk;

	WAVSource src;
	if (!src.open(input_filename, 65536))
	{
		printf("Cannot open file %s (not a PCM file?)\n", input_filename);
		return 1;
	}

	if (!dsound.open_dsound(0, true))
	{
		printf("Error: failed to init DirectSound\n");
		return 1;
	}
	dsound.open(Speakers(FORMAT_SPDIF, MODE_STEREO, 48000));

	/////////////////////////////////////////////////////
	// Setup

	dvd_graph.set_use_spdif(true);
	dvd_graph.proc.set_input_order(std_order);
	dvd_graph.proc.set_output_order(win_order);
	dvd_graph.set_user(Speakers(FORMAT_PCM32, MODE_5_1, 48000));
	dvd_graph.set_input(src.get_output());

	/////////////////////////////////////////////////////
	// Process

	while (!src.is_empty())
	{
		src.get_chunk(&chunk);
		dvd_graph.process(&chunk);

		while (!dvd_graph.is_empty())
		{
			dvd_graph.get_chunk(&chunk);
			if (!chunk.is_dummy())
			{
				sink->process(&chunk);

				if (print_process)
				{
					char info[4096];
					dvd_graph.get_info(info, sizeof(info));
					printf("%s\n", info);
					print_process = false;
				}
			}
		}
	}

	/////////////////////////////////////////////////////
	// Flushing

	chunk.set_empty(dvd_graph.get_input());
	chunk.eos = true;

	if (!dvd_graph.process_to(&chunk, sink))
	{
		printf("\nProcessing error!\n");
		return 1;
	}

	dsound.close_dsound();

	return 0;
}
