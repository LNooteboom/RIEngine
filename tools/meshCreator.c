#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "../include/gfx/draw.h"

#define logNorm printf
/*****************************************
 * MESH
 ****************************************/

struct Vert {
	float v[3];
	float vn[3];
	float vt[2];

	float col[4];
};

static void *getData(cgltf_accessor *acc, unsigned int i) {
	char *data = acc->buffer_view->data;
	if (!data)
		data = acc->buffer_view->buffer->data;
	return data + acc->buffer_view->offset + (i * acc->stride);
}

static int getDataFloat(float *out, cgltf_accessor *acc, unsigned int vert, unsigned int n) {
	union {
		float *f;
		uint16_t *u16;
		void *v;
	} p;
	p.v = getData(acc, vert);
	switch (acc->component_type) {
	case cgltf_component_type_r_32f:
		for (unsigned int i = 0; i < n; i++) {
			out[i] = p.f[i];
		}
		break;
	case cgltf_component_type_r_16u:
		for (unsigned int i = 0; i < n; i++) {
			out[i] = p.u16[i] / (float)0xFFFF;
		}
		break;
	default:
		printf("Unknown component type %d\n", acc->component_type);
		return -1;
	}
	return 0;
}

static int ldmes(cgltf_mesh *mesh, FILE *outfile, const char *name, unsigned int primIndex) {
	cgltf_primitive *prim = &mesh->primitives[primIndex];
	cgltf_attribute *v = NULL, *vn = NULL, *vt = NULL, *vcol = NULL, *vj = NULL, *vw = NULL;
	for (unsigned int a = 0; a < prim->attributes_count; a++) {
		cgltf_attribute *att = &prim->attributes[a];
		switch (att->type) {
			case cgltf_attribute_type_position:
				v = att;
				break;
			case cgltf_attribute_type_normal:
				vn = att;
				break;
			case cgltf_attribute_type_texcoord:
				vt = att;
				break;
			case cgltf_attribute_type_color:
				vcol = att;
				break;
			case cgltf_attribute_type_joints:
				vj = att;
				break;
			case cgltf_attribute_type_weights:
				vw = att;
				break;
			default:
				break; /* Do nothing */
		}
	}
	if (!v || !vn || !vt) {
		printf("Incomplete mesh %s~%d\n", name, primIndex);
		return -1;
	}
	cgltf_accessor *indices = prim->indices;

	struct ModelFileEntry mfe;
	memset(&mfe, 0, sizeof(mfe));
	if (primIndex) {
		snprintf(mfe.name, MODEL_NAME_LEN, "%s~%d", name, primIndex);
	} else {
		strncpy(mfe.name, name, MODEL_NAME_LEN);
	}
	mfe.name[MODEL_NAME_LEN - 1] = 0;
	

	mfe.nVertices = (uint32_t)v->data->count;
	mfe.nTriangles = (uint32_t)indices->count / 3;

	if (vj && vw) {
		mfe.flags = MODEL_FILE_ANIM;
		printf("Mesh (anim): %s\n", mfe.name);
	} else if (vcol) {
		mfe.flags = MODEL_FILE_VCOL;
		printf("Mesh (vcol): %s\n", mfe.name);
	} else {
		printf("Mesh: %s\n", mfe.name);
	}
	fwrite(&mfe, sizeof(mfe), 1, outfile);

	/* Write verts */
	for (unsigned int i = 0; i < mfe.nVertices; i++) {
		struct Vert sv;
		getDataFloat(sv.v, v->data, i, 3);
		getDataFloat(sv.vn, vn->data, i, 3);
		getDataFloat(sv.vt, vt->data, i, 2);

		if (vj && vw) {
			getDataFloat(sv.col, vw->data, i, 4);
		} else if (vcol) {
			getDataFloat(sv.col, vcol->data, i, 4);
		} else {
			sv.col[0] = sv.col[1] = sv.col[2] = 0.5f;
			sv.col[3] = 1.0f;
		}
		fwrite(&sv, sizeof(sv), 1, outfile);
		if (vj && vw) {
			uint8_t joints[4];
			memcpy(joints, getData(vj->data, i), 4);
			fwrite(joints, 1, 4, outfile);
		}
	}

	/* write indices */
	unsigned int *in = malloc(indices->count * 4);
	for (unsigned int i = 0; i < mfe.nTriangles * 3; i++) {
		in[i] = *((uint16_t *)getData(indices, i));
	}
	fwrite(in, 4, indices->count, outfile);
	free(in);

	return 0;
}
static cgltf_node *findNode(cgltf_data *data, cgltf_mesh *m) {
	for (unsigned int i = 0; i < data->nodes_count; i++) {
		cgltf_node *n = &data->nodes[i];
		if (n->mesh == m) {
			return n;
		}
	}
	return NULL;
}

static int exportMesh(cgltf_data *data, const char *outname) {
	FILE *outfile = fopen(outname, "wb");
	struct ModelFileHeader hdr;
	memcpy(hdr.sig, "MES0", 4);
	hdr.nEntries = 0;
	fwrite(&hdr, sizeof(hdr), 1, outfile);

	int err = 0;
	for (unsigned int m = 0; m < data->meshes_count; m++) {
		cgltf_mesh *mesh = &data->meshes[m];
		cgltf_node *n = findNode(data, mesh);
		const char *name;
		if (n && n->name) {
			name = n->name;
		} else {
			name = mesh->name;
		}

		for (unsigned int p = 0; p < mesh->primitives_count; p++) {
			err = ldmes(mesh, outfile, name, p);
			hdr.nEntries++;
			if (err)
				break;
		}

		if (err)
			break;
	}
	
	rewind(outfile);
	fwrite(&hdr, sizeof(hdr), 1, outfile);

	fclose(outfile);
	return err;
}


/*****************************************
 * POSE
 ****************************************/

static unsigned int findBoneIdx(cgltf_skin *skin, cgltf_node *node) {
	for (unsigned int i = 0; i < skin->joints_count; i++) {
		if (skin->joints[i] == node)
			return i;
	}
	return 0;
}
static enum PoseFileAnimChannelType getAnimChannelType(cgltf_animation_channel *ch) {
	switch (ch->target_path) {
	case cgltf_animation_path_type_translation:
		return POSE_TRANSLATE;
	case cgltf_animation_path_type_rotation:
		return POSE_ROTATE;
	case cgltf_animation_path_type_scale:
		return POSE_SCALE;
	default:
		return -1;
	}
}
static int findParent(cgltf_skin *skin, cgltf_node *joint) {
	for (unsigned int i = 0; i < skin->joints_count; i++) {
		cgltf_node *j = skin->joints[i];
		for (unsigned int c = 0; c < j->children_count; c++) {
			if (j->children[c] == joint) {
				return i;
			}
		}
	}
	return -1;
}
static size_t getOutputChannelSize(cgltf_animation_channel *ch) {
	return sizeof(struct PoseFileAnimChannel) + ch->sampler->input->count * (ch->target_path == cgltf_animation_path_type_rotation ? sizeof(struct PoseFileVec4) : sizeof(struct PoseFileVec3));
}


static int exportPose(cgltf_data *data, const char *outname) {
	if (data->animations_count == 0 || data->skins_count == 0) {
		printf("No animations!\n");
		return -1;
	}
	if (data->skins_count > 1) {
		printf("More than one skins in file, exporting only the first one\n");
	}
	cgltf_skin *skin = data->skins;

	FILE *outfile = fopen(outname, "wb");

	/* Write header */
	struct PoseFileHeader hdr;
	memcpy(hdr.sig, "POSE", 4);
	hdr.version = 0x100;
	hdr.nBones = (unsigned int)skin->joints_count;
	hdr.nAnim = (unsigned int)data->animations_count;
	fwrite(&hdr, sizeof(hdr), 1, outfile);

	/* Write bones */
	for (unsigned int i = 0; i < hdr.nBones; i++) {
		struct PoseFileBone bone;
		cgltf_node *joint = skin->joints[i];
		bone.parent = findParent(skin, joint);
		memcpy(bone.inverseBindMat, getData(skin->inverse_bind_matrices, i), sizeof(float) * 16);
		if (joint->has_translation) {
			memcpy(bone.trans, joint->translation, sizeof(float) * 3);
		} else {
			memset(bone.trans, 0, sizeof(float) * 3);
		}
		if (joint->has_rotation) {
			memcpy(bone.rot, joint->rotation, sizeof(float) * 4);
		} else {
			memset(bone.rot, 0, sizeof(float) * 3);
			bone.rot[3] = 1;
		}
		if (joint->has_scale) {
			memcpy(bone.scale, joint->scale, sizeof(float) * 3);
		} else {
			bone.scale[0] = bone.scale[1] = bone.scale[2] = 1.0f;
		}
		fwrite(&bone, sizeof(bone), 1, outfile);
	}

	/* Write anims */
	for (unsigned int i = 0; i < hdr.nAnim; i++) {
		cgltf_animation *anim = &data->animations[i];
		struct PoseFileAnim an;
		strncpy(an.name, anim->name, 32);
		an.name[31] = 0;
		an.nChannels = (unsigned int)anim->channels_count;
		an.size = sizeof(an);
		an.duration = 0;
		for (unsigned int j = 0; j < an.nChannels; j++) {
			an.size += getOutputChannelSize(&anim->channels[j]);
			if (anim->channels[j].sampler->input->has_max) {
				float max = anim->channels[j].sampler->input->max[0];
				if (max > an.duration) {
					an.duration = max;
				}
			}
		}
		printf("Anim: %s (%f seconds)\n", an.name, an.duration);
		fwrite(&an, sizeof(an), 1, outfile);

		/* Write anim channel */
		for (unsigned int j = 0; j < an.nChannels; j++) {
			cgltf_animation_channel *ach = &anim->channels[j];
			struct PoseFileAnimChannel ch;
			ch.bone = findBoneIdx(skin, ach->target_node);
			ch.type = getAnimChannelType(ach);
			ch.nEntries = (unsigned int)ach->sampler->input->count;
			ch.size = getOutputChannelSize(ach);
			fwrite(&ch, sizeof(ch), 1, outfile);

			for (unsigned int k = 0; k < ch.nEntries; k++) {
				if (ch.type == POSE_ROTATE) {
					struct PoseFileVec4 v;
					v.t = *((float *)getData(ach->sampler->input, k));
					float *f = getData(ach->sampler->output, k);
					v.v[0] = f[0];
					v.v[1] = f[1];
					v.v[2] = f[2];
					v.v[3] = f[3];
					fwrite(&v, sizeof(v), 1, outfile);
				} else {
					struct PoseFileVec3 v;
					v.t = *((float *)getData(ach->sampler->input, k));
					float *f = getData(ach->sampler->output, k);
					v.v[0] = f[0];
					v.v[1] = f[1];
					v.v[2] = f[2];
					fwrite(&v, sizeof(v), 1, outfile);
				}
			}
		}
	}

	fclose(outfile);
	return 0;
}

/*****************************************
 * nodes
 ****************************************/

static int printScene(cgltf_data *data) {
	for (unsigned int n = 0; n < data->nodes_count; n++) {
		cgltf_node *node = &data->nodes[n];
		if (node->mesh) {
			for (unsigned int p = 0; p < node->mesh->primitives_count; p++) {
				const cgltf_material *mat = node->mesh->primitives[p].material;
				const char *texName = mat ? mat->name : "";
				if (p) {
					printf("mesh(\"%s~%d\", \"tex/3d/%s.png\", %f, %f, %f, %f); \n", node->name, p, texName, node->translation[0], node->translation[1], node->translation[2], 0.0f);
				} else {
					printf("mesh(\"%s\", \"tex/3d/%s.png\", %f, %f, %f, %f);\n", node->name, texName, node->translation[0], node->translation[1], node->translation[2], 0.0f);
				}
			}
		}
	}

	return 0;
}

 /*****************************************
  * main
  ****************************************/

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage: meshCreator [mesh, pose or scene] <input gltf> [output file]\n");
		return -1;
	}
	cgltf_options options = {0};
	cgltf_data *data;
	cgltf_result result = cgltf_parse_file(&options, argv[2], &data);
	cgltf_load_buffers(&options, data, argv[2]);
	if (result != cgltf_result_success) {
		printf("Failed to parse model file %s\n", argv[2]);
		return -1;
	}

	int err = -1;
	if (!strcmp(argv[1], "mesh")) {
		if (argc != 4) {
			printf("Invalid number of arguments\n");
			return -1;
		}
		err = exportMesh(data, argv[3]);
	} else if (!strcmp(argv[1], "pose")) {
		if (argc != 4) {
			printf("Invalid number of arguments\n");
			return -1;
		}
		err = exportPose(data, argv[3]);
	} else if (!strcmp(argv[1], "scene")) {
		if (argc != 3) {
			printf("Invalid number of arguments\n");
				return -1;
		}
		err = printScene(data);
	} else {
		printf("Unknown command: %s\n", argv[1]);
	}

	cgltf_free(data);

	return err;
}
