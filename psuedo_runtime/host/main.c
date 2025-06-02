#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* For the UUID (found in the TA's h-file(s)) */
#include <psuedo_runtime_ta.h>

char *bh_read_file_to_buffer(const char *filename, uint32_t *ret_size);

char *bh_read_file_to_buffer(const char *filename, uint32_t *ret_size)
{
    char *buffer;
    int file;
    uint32_t file_size, buf_size, read_size;
    struct stat stat_buf;

    if (!filename || !ret_size)
    {
        printf("Read file to buffer failed: invalid filename or ret size.\n");
        return NULL;
    }

    if ((file = open(filename, O_RDONLY, 0)) == -1)
    {
        printf("Read file to buffer failed: open file %s failed.\n", filename);
        return NULL;
    }

    if (fstat(file, &stat_buf) != 0)
    {
        printf("Read file to buffer failed: fstat file %s failed.\n", filename);
        close(file);
        return NULL;
    }

    file_size = (uint32_t)stat_buf.st_size;

    /* At lease alloc 1 byte to avoid malloc failed */
    buf_size = file_size > 0 ? file_size : 1;

    if (!(buffer = malloc(buf_size)))
    {
        printf("Read file to buffer failed: alloc memory failed.\n");
        close(file);
        return NULL;
    }
#if WASM_ENABLE_MEMORY_TRACING != 0
    printf("Read file, total size: %u\n", file_size);
#endif

    read_size = (uint32_t)read(file, buffer, file_size);
    close(file);

    if (read_size < file_size)
    {
        printf("Read file to buffer failed: read file content failed.\n");
        free(buffer);
        return NULL;
    }

    *ret_size = file_size;
    return buffer;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("invalid argument\n");
        return 1;
    }
    char *filename = argv[1];

    uint32_t *ret_size;
    char *res = bh_read_file_to_buffer(filename, ret_size);
    printf("%s\n", res);

    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_PSUEDO_RUNTIME_UUID;
    uint32_t err_origin;

    /* Initialize a context connecting us to the TEE */
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

    /*
     * Open a session to the "psuedo runtime" TA, the TA will print "hello
     * world!" in the log when the session is created.
     */
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                           TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
             res, err_origin);

    /*
     * Execute a function in the TA by invoking it, in this case
     * we're incrementing a number.
     *
     * The value of command ID part and how the parameters are
     * interpreted is part of the interface provided by the TA.
     */

    /* Clear the TEEC_Operation struct */
    memset(&op, 0, sizeof(op));

    /*
     * Prepare the argument. Pass a value in the first parameter,
     * the remaining three parameters are unused.
     */
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE,
                                     TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = 42;

    /*
     * TA_PSUEDO_RUNTIME_CMD_INC_VALUE is the actual function in the TA to be
     * called.
     */
    printf("Invoking TA to increment %d\n", op.params[0].value.a);
    res = TEEC_InvokeCommand(&sess, TA_PSUEDO_RUNTIME_CMD_INC_VALUE, &op,
                             &err_origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x",
             res, err_origin);
    printf("TA incremented value to %d\n", op.params[0].value.a);

    /*
     * We're done with the TA, close the session and
     * destroy the context.
     *
     * The TA will print "Goodbye!" in the log when the
     * session is closed.
     */

    TEEC_CloseSession(&sess);

    TEEC_FinalizeContext(&ctx);

    return 0;
}
