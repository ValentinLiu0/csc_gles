
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/dma-heap.h>

#include <csc_gles.h>
#include <csc_gles_log.h>

#define CSC_GLES_TEST_ANDROID_VERSION "1.0.0"

static int create_dmabuf(size_t size, void **mapped)
{
	int heap_fd = open("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);
	if (heap_fd < 0) {
		LOG_E("Failed to open DMA heap: %s\n", strerror(errno));
		return -1;
	}

	struct dma_heap_allocation_data data = {
		.len = size,
		.fd = 0,
		.fd_flags = O_RDWR | O_CLOEXEC,
		.heap_flags = 0,
	};

	if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data) < 0) {
		LOG_E("DMA-BUF allocation failed: %s\n", strerror(errno));
		close(heap_fd);
		return -1;
	}

	close(heap_fd);

	void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
	                 MAP_SHARED, (int)data.fd, 0);
	if (ptr == MAP_FAILED) {
		LOG_E("DMA-BUF mmap failed: %s\n", strerror(errno));
		close((int)data.fd);
		return -1;
	}

	*mapped = ptr;
	return (int)data.fd;
}

/**
 * csc_gles_test_android <width> <height> <input_rgba_file> <output_nv12_file>
 */
int main(int argc, char **argv)
{
	LOG_I("Version: %s\n", CSC_GLES_TEST_ANDROID_VERSION);
	if (argc != 5) {
		LOG_E("Usage: csc_gles_test_simple <width> <height> <input_rgba_file> <output_nv12_file>\n");
		return 0;
	}

	int width = atoi(argv[1]);
	int height = atoi(argv[2]);
	const char *input_rgba_file = argv[3];
	const char *output_nv12_file = argv[4];

	if (width <= 0 || height <= 0) {
		LOG_E("Invalid input params.");
		return 1;
	}

	FILE *in_fp = fopen(input_rgba_file, "rb");
	if (!in_fp) {
		LOG_E("Failed to open input file.\n");
		return 1;
	}
	const size_t rgba_data_size = width * height *4;
	//uint8_t *rgba_data = (uint8_t *) malloc(rgba_data_size);
	void *rgba_data;
	int dma_fd = create_dmabuf(rgba_data_size, &rgba_data);
	fread(rgba_data, 1, rgba_data_size, in_fp);
	fflush(in_fp);
	fclose(in_fp);

	csc_gles_init();

	csc_gles_check_ext("GL_EXT_memory_object");
	csc_gles_check_ext("GL_EXT_semaphore");
	csc_gles_check_ext("GL_EXT_memory_object_fd");
	csc_gles_check_ext("GL_EXT_semaphore_fd");

	csc_gles_imported_buffer_t *buffer = nullptr;
	//csc_gles_import_mem(CSC_GLES_BUFFER_TYPE_RGBA8888, width, height, rgba_data, &buffer);
	csc_gles_import_dmabuf(CSC_GLES_BUFFER_TYPE_RGBA8888, width, height, dma_fd, &buffer);
	GLuint ssbo;
	size_t nv12_data_size;
	csc_gles_convert_rgba8888_to_nv12(buffer, &ssbo, &nv12_data_size);

	void *mapped = nullptr;
	csc_gles_ssbo_map(ssbo, nv12_data_size, &mapped);

	FILE *out_fp = fopen(output_nv12_file, "wb");
	if (!out_fp) {
		LOG_E("Failed to open output file.\n");
		return 1;
	}
	fwrite(mapped, 1, nv12_data_size, out_fp);
	fflush(out_fp);
	fclose(out_fp);

	csc_gles_ssbo_unmap(ssbo);

	csc_gles_deinit();

	return 0;
}