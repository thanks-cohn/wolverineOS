/*
 * bind.c
 * Creates a robust .imeta sidecar for a target file.
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

int imeta_hash_file_fnv1a64(
    const char *path,
    char *out_hex,
    size_t out_hex_size
);

static int make_sidecar_path(const char *file_path, char *out_path, size_t out_path_size) {
    int written = snprintf(out_path, out_path_size, "%s.imeta", file_path);

    if (written < 0 || (size_t)written >= out_path_size) {
        fprintf(stderr, "[BIND][ERROR] sidecar path too long for: %s\n", file_path);
        return 1;
    }

    return 0;
}

static void write_json_string(FILE *fp, const char *value) {
    fputc('"', fp);

    if (!value) {
        fputc('"', fp);
        return;
    }

    for (const char *p = value; *p; p++) {
        switch (*p) {
            case '\\': fputs("\\\\", fp); break;
            case '"':  fputs("\\\"", fp); break;
            case '\n': fputs("\\n", fp); break;
            case '\r': fputs("\\r", fp); break;
            case '\t': fputs("\\t", fp); break;
            default:   fputc(*p, fp); break;
        }
    }

    fputc('"', fp);
}

int imeta_bind_file(const char *file_path) {
    if (!file_path || strlen(file_path) == 0) {
        fprintf(stderr, "[BIND][ERROR] no file path provided\n");
        return 1;
    }

    fprintf(stderr, "[BIND] starting bind for: %s\n", file_path);

    struct stat st;

    if (stat(file_path, &st) != 0) {
        fprintf(stderr, "[BIND][ERROR] stat failed: %s: %s\n", file_path, strerror(errno));
        return 1;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "[BIND][ERROR] target is not a regular file: %s\n", file_path);
        return 1;
    }

    char hash_hex[32];
    memset(hash_hex, 0, sizeof(hash_hex));

    if (imeta_hash_file_fnv1a64(file_path, hash_hex, sizeof(hash_hex)) != 0) {
        fprintf(stderr, "[BIND][ERROR] failed to hash target file: %s\n", file_path);
        return 1;
    }

    char sidecar_path[PATH_MAX];

    if (make_sidecar_path(file_path, sidecar_path, sizeof(sidecar_path)) != 0) {
        return 1;
    }

    struct stat sidecar_st;

    if (stat(sidecar_path, &sidecar_st) == 0) {
        fprintf(stderr, "[BIND][WARN] sidecar already exists and will be overwritten: %s\n", sidecar_path);
    }

    FILE *fp = fopen(sidecar_path, "w");

    if (!fp) {
        fprintf(stderr, "[BIND][ERROR] failed to open sidecar for writing: %s: %s\n",
                sidecar_path,
                strerror(errno));
        return 1;
    }

    time_t now = time(NULL);

    fprintf(fp, "{\n");
    fprintf(fp, "  \"imeta_version\": \"0.2.0\",\n");
    fprintf(fp, "  \"kind\": \"sidecar\",\n");
    fprintf(fp, "  \"schema\": {\n");
    fprintf(fp, "    \"name\": \"imeta.image_training_sidecar\",\n");
    fprintf(fp, "    \"purpose\": \"portable metadata, annotation, image-pairing, and training export preparation\",\n");
    fprintf(fp, "    \"json_ready\": true\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"asset\": {\n");
    fprintf(fp, "    \"path\": ");
    write_json_string(fp, file_path);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"sidecar_path\": ");
    write_json_string(fp, sidecar_path);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"size_bytes\": %lld,\n", (long long)st.st_size);
    fprintf(fp, "    \"inode\": %lld,\n", (long long)st.st_ino);
    fprintf(fp, "    \"mtime\": %lld,\n", (long long)st.st_mtime);
    fprintf(fp, "    \"hash_fnv1a64\": ");
    write_json_string(fp, hash_hex);
    fprintf(fp, "\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"identity\": {\n");
    fprintf(fp, "    \"hash_method\": \"fnv1a64\",\n");
    fprintf(fp, "    \"hash\": ");
    write_json_string(fp, hash_hex);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"strong_hash_method\": \"\",\n");
    fprintf(fp, "    \"strong_hash\": \"\"\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"annotation\": {\n");
    fprintf(fp, "    \"description\": \"\",\n");
    fprintf(fp, "    \"short_caption\": \"\",\n");
    fprintf(fp, "    \"long_caption\": \"\",\n");
    fprintf(fp, "    \"tags\": [],\n");
    fprintf(fp, "    \"features\": {\n");
    fprintf(fp, "      \"subjects\": [],\n");
    fprintf(fp, "      \"people\": [],\n");
    fprintf(fp, "      \"objects\": [],\n");
    fprintf(fp, "      \"colors\": [],\n");
    fprintf(fp, "      \"clothing\": [],\n");
    fprintf(fp, "      \"body\": {},\n");
    fprintf(fp, "      \"face\": {},\n");
    fprintf(fp, "      \"hair\": {},\n");
    fprintf(fp, "      \"eyes\": {}\n");
    fprintf(fp, "    },\n");
    fprintf(fp, "    \"includes\": [],\n");
    fprintf(fp, "    \"actions\": [],\n");
    fprintf(fp, "    \"setting\": \"\",\n");
    fprintf(fp, "    \"style\": [],\n");
    fprintf(fp, "    \"quality_notes\": \"\",\n");
    fprintf(fp, "    \"notes\": \"\"\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"image_pairing\": {\n");
    fprintf(fp, "    \"enabled\": true,\n");
    fprintf(fp, "    \"pair_type\": \"image_metadata\",\n");
    fprintf(fp, "    \"caption_file_extension\": \".txt\",\n");
    fprintf(fp, "    \"metadata_file_extension\": \".json\",\n");
    fprintf(fp, "    \"preferred_caption_fields\": [\n");
    fprintf(fp, "      \"lora.caption\",\n");
    fprintf(fp, "      \"stable_diffusion.prompt\",\n");
    fprintf(fp, "      \"training.caption\",\n");
    fprintf(fp, "      \"annotation.long_caption\",\n");
    fprintf(fp, "      \"annotation.short_caption\",\n");
    fprintf(fp, "      \"annotation.description\",\n");
    fprintf(fp, "      \"annotation.tags\"\n");
    fprintf(fp, "    ]\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"training\": {\n");
    fprintf(fp, "    \"enabled\": false,\n");
    fprintf(fp, "    \"type\": \"none\",\n");
    fprintf(fp, "    \"split\": \"train\",\n");
    fprintf(fp, "    \"caption\": \"\",\n");
    fprintf(fp, "    \"negative_caption\": \"\",\n");
    fprintf(fp, "    \"label\": \"\",\n");
    fprintf(fp, "    \"class_name\": \"\",\n");
    fprintf(fp, "    \"quality\": \"\",\n");
    fprintf(fp, "    \"weight\": 1.0,\n");
    fprintf(fp, "    \"exclude\": false,\n");
    fprintf(fp, "    \"reason_excluded\": \"\"\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"stable_diffusion\": {\n");
    fprintf(fp, "    \"prompt\": \"\",\n");
    fprintf(fp, "    \"negative_prompt\": \"\",\n");
    fprintf(fp, "    \"model\": \"\",\n");
    fprintf(fp, "    \"model_hash\": \"\",\n");
    fprintf(fp, "    \"vae\": \"\",\n");
    fprintf(fp, "    \"sampler\": \"\",\n");
    fprintf(fp, "    \"scheduler\": \"\",\n");
    fprintf(fp, "    \"steps\": null,\n");
    fprintf(fp, "    \"cfg_scale\": null,\n");
    fprintf(fp, "    \"seed\": null,\n");
    fprintf(fp, "    \"width\": null,\n");
    fprintf(fp, "    \"height\": null,\n");
    fprintf(fp, "    \"clip_skip\": null,\n");
    fprintf(fp, "    \"denoising_strength\": null,\n");
    fprintf(fp, "    \"loras_used\": [],\n");
    fprintf(fp, "    \"embeddings_used\": [],\n");
    fprintf(fp, "    \"controlnet\": [],\n");
    fprintf(fp, "    \"workflow\": {}\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"lora\": {\n");
    fprintf(fp, "    \"enabled\": false,\n");
    fprintf(fp, "    \"caption\": \"\",\n");
    fprintf(fp, "    \"trigger_tokens\": [],\n");
    fprintf(fp, "    \"class_tokens\": [],\n");
    fprintf(fp, "    \"instance_tokens\": [],\n");
    fprintf(fp, "    \"regularization\": false,\n");
    fprintf(fp, "    \"repeat_count\": 1,\n");
    fprintf(fp, "    \"caption_fields\": [\n");
    fprintf(fp, "      \"lora.trigger_tokens\",\n");
    fprintf(fp, "      \"lora.instance_tokens\",\n");
    fprintf(fp, "      \"lora.class_tokens\",\n");
    fprintf(fp, "      \"lora.caption\",\n");
    fprintf(fp, "      \"annotation.description\",\n");
    fprintf(fp, "      \"annotation.features\",\n");
    fprintf(fp, "      \"annotation.includes\",\n");
    fprintf(fp, "      \"annotation.actions\",\n");
    fprintf(fp, "      \"annotation.setting\",\n");
    fprintf(fp, "      \"annotation.tags\",\n");
    fprintf(fp, "      \"annotation.style\"\n");
    fprintf(fp, "    ],\n");
    fprintf(fp, "    \"caption_separator\": \", \",\n");
    fprintf(fp, "    \"caption_extension\": \".txt\",\n");
    fprintf(fp, "    \"output_mode\": \"paired_caption_files\",\n");
    fprintf(fp, "    \"export_folder_name\": \"lora_training_pair\"\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"llm\": {\n");
    fprintf(fp, "    \"enabled\": false,\n");
    fprintf(fp, "    \"format\": \"jsonl\",\n");
    fprintf(fp, "    \"instruction\": \"\",\n");
    fprintf(fp, "    \"input\": \"\",\n");
    fprintf(fp, "    \"output\": \"\",\n");
    fprintf(fp, "    \"messages\": []\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"export_profiles\": {\n");
    fprintf(fp, "    \"generic_jsonl\": {\n");
    fprintf(fp, "      \"enabled\": true,\n");
    fprintf(fp, "      \"output\": \".imeta/exports/generic/export.jsonl\"\n");
    fprintf(fp, "    },\n");
    fprintf(fp, "    \"lora\": {\n");
    fprintf(fp, "      \"enabled\": true,\n");
    fprintf(fp, "      \"source_block\": \"lora\",\n");
    fprintf(fp, "      \"output_mode\": \"paired_caption_files\",\n");
    fprintf(fp, "      \"output\": \".imeta/exports/lora_training_pair\"\n");
    fprintf(fp, "    },\n");
    fprintf(fp, "    \"stable_diffusion\": {\n");
    fprintf(fp, "      \"enabled\": true,\n");
    fprintf(fp, "      \"source_block\": \"stable_diffusion\",\n");
    fprintf(fp, "      \"output\": \".imeta/exports/stable_diffusion\"\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"visual_shard\": {\n");
    fprintf(fp, "    \"enabled\": false,\n");
    fprintf(fp, "    \"grid\": \"8-region\",\n");
    fprintf(fp, "    \"colors\": [],\n");
    fprintf(fp, "    \"rare_colors\": [],\n");
    fprintf(fp, "    \"region_summaries\": []\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"custom\": {},\n");
    fprintf(fp, "  \"created_unix\": %lld,\n", (long long)now);
    fprintf(fp, "  \"updated_unix\": %lld\n", (long long)now);
    fprintf(fp, "}\n");

    fclose(fp);

    fprintf(stderr, "[BIND] wrote robust sidecar: %s\n", sidecar_path);
    fprintf(stderr, "[BIND] bind completed successfully\n");

    return 0;
}
