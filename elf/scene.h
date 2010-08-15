
const char* compose_main_vert =
"attribute vec3 elf_VertexAttr;\n"
"attribute vec2 elf_TexCoordAttr;\n"
"uniform mat4 elf_ModelviewMatrix;\n"
"uniform mat4 elf_ProjectionMatrix;\n"
"varying vec2 elf_TexCoord;\n"
"void main()\n"
"{\n"
"\telf_TexCoord = elf_TexCoordAttr;\n"
"\tgl_Position = elf_ProjectionMatrix*(elf_ModelviewMatrix*vec4(elf_VertexAttr, 1.0))\n;"
"}\n";

const char* compose_main_frag =
"uniform sampler2D elf_Texture0;\n"
"uniform sampler2D elf_Texture1;\n"
"uniform sampler2D elf_Texture2;\n"
"varying vec2 elf_TexCoord;\n"
"void main()\n"
"{\n"
"\tvec3 diffuse = texture2D(elf_Texture1, elf_TexCoord).rgb;\n"
"\tvec3 specular = texture2D(elf_Texture2, elf_TexCoord).rgb;\n"
"\tvec3 color = texture2D(elf_Texture0, elf_TexCoord).rgb;\n"
"\tgl_FragColor = vec4(color*diffuse+specular, 1.0);\n"
"}\n";

elf_scene* elf_create_scene(const char *name)
{
	elf_scene *scene;

	scene = (elf_scene*)malloc(sizeof(elf_scene));
	memset(scene, 0x0, sizeof(elf_scene));
	scene->obj_type = ELF_SCENE;
	scene->obj_destr = elf_destroy_scene;

	scene->models = elf_create_list();
	scene->scripts = elf_create_list();
	scene->textures = elf_create_list();
	scene->materials = elf_create_list();
	scene->cameras = elf_create_list();
	scene->entities = elf_create_list();
	scene->lights = elf_create_list();
	scene->armatures = elf_create_list();
	scene->particles = elf_create_list();
	scene->sprites = elf_create_list();
	scene->entity_queue = elf_create_list();
	scene->sprite_queue = elf_create_list();

	elf_inc_ref((elf_object*)scene->models);
	elf_inc_ref((elf_object*)scene->scripts);
	elf_inc_ref((elf_object*)scene->materials);
	elf_inc_ref((elf_object*)scene->textures);
	elf_inc_ref((elf_object*)scene->cameras);
	elf_inc_ref((elf_object*)scene->entities);
	elf_inc_ref((elf_object*)scene->lights);
	elf_inc_ref((elf_object*)scene->armatures);
	elf_inc_ref((elf_object*)scene->particles);
	elf_inc_ref((elf_object*)scene->sprites);
	elf_inc_ref((elf_object*)scene->entity_queue);
	elf_inc_ref((elf_object*)scene->sprite_queue);

	gfx_set_shader_params_default(&scene->shader_params);

	scene->world = elf_create_physics_world();
	scene->dworld = elf_create_physics_world();

	scene->physics = ELF_TRUE;
	scene->run_scripts = ELF_TRUE;

	if(name) scene->name = elf_create_string(name);

	scene->compose_main_shdr = gfx_create_shader_program(compose_main_vert, compose_main_frag);

	scene->id = ++gen->scene_id_counter;

	elf_inc_obj(ELF_SCENE);

	return scene;
}

void elf_recursively_import_assets(elf_scene *scene, const struct aiScene *aiscn, struct aiNode *aind)
{
	int i, j, k;
	elf_entity *entity;
	elf_model *model;
	elf_material *material;
	elf_texture *texture;
	int area_index;
	int vertex_offset;
	int index_offset;
	float *vertex_buffer;
	float *normal_buffer;
	float *texcoord_buffer;
	unsigned int *index_buffer;
	struct aiColor4D col;
	const struct aiMaterial *aimat;
	float shininess;
	float strength;
	unsigned int max;
	struct aiString path;
	struct aiString name;
	char *parent_folder;
	char *real_path;
	unsigned char is_tex_coords;

	entity = elf_create_entity(aind->mName.data);
	model = elf_create_model(aind->mName.data);

	model->frame_count = 1;
	is_tex_coords = ELF_FALSE;

	for(i = 0; i < (int)aind->mNumMeshes; i++)
	{
		const struct aiMesh *mesh = aiscn->mMeshes[aind->mMeshes[i]];

		if(mesh->mNumVertices < 3 || mesh->mNumFaces < 1) continue;

		model->vertice_count += mesh->mNumVertices;
		model->area_count += 1;
		model->indice_count += mesh->mNumFaces*3;

		if(mesh->mTextureCoords[0] != NULL) is_tex_coords = ELF_TRUE;
	}

	if(model->vertice_count > 2 && model->indice_count > 2)
	{
		model->vertices = gfx_create_vertex_data(3*model->vertice_count, GFX_FLOAT, GFX_VERTEX_DATA_STATIC);
		gfx_inc_ref((gfx_object*)model->vertices);
		vertex_buffer = (float*)gfx_get_vertex_data_buffer(model->vertices);

		model->normals = gfx_create_vertex_data(3*model->vertice_count, GFX_FLOAT, GFX_VERTEX_DATA_STATIC);
		gfx_inc_ref((gfx_object*)model->normals);
		normal_buffer = (float*)gfx_get_vertex_data_buffer(model->normals);

		if(is_tex_coords)
		{
			model->tex_coords = gfx_create_vertex_data(2*model->vertice_count, GFX_FLOAT, GFX_VERTEX_DATA_STATIC);
			gfx_inc_ref((gfx_object*)model->tex_coords);
			texcoord_buffer = (float*)gfx_get_vertex_data_buffer(model->tex_coords);
		}

		model->areas = (elf_model_area*)malloc(sizeof(elf_model_area)*model->area_count);
		memset(model->areas, 0x0, sizeof(elf_model_area)*model->area_count);

		area_index = 0;
		vertex_offset = 0;

		for(i = 0; i < (int)aind->mNumMeshes; i++)
		{
			const struct aiMesh *mesh = aiscn->mMeshes[aind->mMeshes[i]];

			if(mesh->mNumVertices < 3 || mesh->mNumFaces < 1) continue;

			for(j = 0; j < (int)mesh->mNumVertices; j++)
			{
				memcpy(&vertex_buffer[(vertex_offset+j)*3], &mesh->mVertices[j].x, sizeof(float)*3);
				if(mesh->mNormals != NULL)
					memcpy(&normal_buffer[(vertex_offset+j)*3], &mesh->mNormals[j].x, sizeof(float)*3);
				if(mesh->mTextureCoords[0] != NULL)
					memcpy(&texcoord_buffer[(vertex_offset+j)*2], &mesh->mTextureCoords[0][j].x, sizeof(float)*2);
			}

			model->areas[area_index].indice_count = mesh->mNumFaces*3;
			model->areas[area_index].index = gfx_create_vertex_data(model->areas[area_index].indice_count, GFX_UINT, GFX_VERTEX_DATA_STATIC);
			gfx_inc_ref((gfx_object*)model->areas[area_index].index);
			index_buffer = (unsigned int*)gfx_get_vertex_data_buffer(model->areas[area_index].index);

			for(j = 0; j < (int)mesh->mNumFaces; j++)
			{
				const struct aiFace *face = &mesh->mFaces[j];

				if(face->mNumIndices != 3)
				{
					elf_set_error(ELF_INVALID_MESH, "error: unexpected non triangular face when loading mesh\n");
					continue;
				}

				for(k = 0; k < (int)face->mNumIndices; k++)
				{
					index_buffer[j*3+k] = vertex_offset+face->mIndices[k];
				}
			}

			vertex_offset += mesh->mNumVertices;

			model->areas[area_index].vertex_index = gfx_create_vertex_index(GFX_TRUE, model->areas[area_index].index);
			gfx_inc_ref((gfx_object*)model->areas[area_index].vertex_index);

			area_index += 1;

			aimat = aiscn->mMaterials[mesh->mMaterialIndex];

			aiGetMaterialString(aimat, AI_MATKEY_NAME, &name);

			material = elf_create_material(name.data);

			if(AI_SUCCESS == aiGetMaterialColor(aimat, AI_MATKEY_COLOR_DIFFUSE, &col))
				elf_set_material_diffuse_color(material, col.r, col.g, col.b, col.a);
			if(AI_SUCCESS == aiGetMaterialColor(aimat, AI_MATKEY_COLOR_SPECULAR, &col))
				elf_set_material_specular_color(material, col.r, col.g, col.b, col.a);
			if(AI_SUCCESS == aiGetMaterialColor(aimat, AI_MATKEY_COLOR_AMBIENT, &col))
				elf_set_material_ambient_color(material, col.r, col.g, col.b, col.a);

			max = 1;
			if(AI_SUCCESS == aiGetMaterialFloatArray(aimat, AI_MATKEY_SHININESS, &shininess, &max))
			{
				max = 1;
				if(AI_SUCCESS == aiGetMaterialFloatArray(aimat, AI_MATKEY_SHININESS_STRENGTH, &strength, &max))
				{
					elf_set_material_specular_power(material, shininess*strength);
				}
				else
				{
					elf_set_material_specular_power(material, shininess);
				}
			}

			parent_folder = elf_get_directory_from_path(scene->file_path);
			real_path = NULL;

			if(AI_SUCCESS == aiGetMaterialString(aimat, AI_MATKEY_TEXTURE_DIFFUSE(0), &path))
			{
				if(real_path) elf_destroy_string(real_path);
				real_path = elf_merge_strings(parent_folder, path.data);
				texture = elf_create_texture_from_file(real_path);
				if(texture) elf_set_material_diffuse_map(material, texture);
			}

			if(AI_SUCCESS == aiGetMaterialString(aimat, AI_MATKEY_TEXTURE_SPECULAR(0), &path))
			{
				if(real_path) elf_destroy_string(real_path);
				real_path = elf_merge_strings(parent_folder, path.data);
				texture = elf_create_texture_from_file(real_path);
				if(texture) elf_set_material_specular_map(material, texture);
			}

			if(AI_SUCCESS == aiGetMaterialString(aimat, AI_MATKEY_TEXTURE_NORMALS(0), &path))
			{
				if(real_path) elf_destroy_string(real_path);
				real_path = elf_merge_strings(parent_folder, path.data);
				texture = elf_create_texture_from_file(real_path);
				if(texture) elf_set_material_normal_map(material, texture);
			}

			if(AI_SUCCESS == aiGetMaterialString(aimat, AI_MATKEY_TEXTURE_LIGHTMAP(0), &path))
			{
				if(real_path) elf_destroy_string(real_path);
				real_path = elf_merge_strings(parent_folder, path.data);
				texture = elf_create_texture_from_file(real_path);
				if(texture) elf_set_material_light_map(material, texture);
			}

			if(AI_SUCCESS == aiGetMaterialString(aimat, AI_MATKEY_TEXTURE_HEIGHT(0), &path))
			{
				if(real_path) elf_destroy_string(real_path);
				real_path = elf_merge_strings(parent_folder, path.data);
				texture = elf_create_texture_from_file(real_path);
				if(texture) elf_set_material_height_map(material, texture);
			}

			elf_destroy_string(parent_folder);
			if(real_path) elf_destroy_string(real_path);

			elf_add_entity_material(entity, material);
		}

		model->vertex_array = gfx_create_vertex_array(GFX_TRUE);
		gfx_inc_ref((gfx_object*)model->vertex_array);

		gfx_set_vertex_array_data(model->vertex_array, GFX_VERTEX, model->vertices);
		gfx_set_vertex_array_data(model->vertex_array, GFX_NORMAL, model->normals);
		if(is_tex_coords) gfx_set_vertex_array_data(model->vertex_array, GFX_TEX_COORD, model->tex_coords);

		model->index = (unsigned int*)malloc(sizeof(unsigned int)*model->indice_count);

		index_offset = 0;

		for(i = 0; i < model->area_count; i++)
		{
			index_buffer = (unsigned int*)gfx_get_vertex_data_buffer(model->areas[i].index);
			memcpy(&model->index[index_offset], index_buffer, sizeof(unsigned int)*model->areas[i].indice_count);
			index_offset += model->areas[i].indice_count;
		}

		// get bounding box values
		memcpy(&model->bb_min.x, vertex_buffer, sizeof(float)*3);
		memcpy(&model->bb_max.x, vertex_buffer, sizeof(float)*3);

		for(i = 3; i < model->vertice_count*3; i+=3)
		{
			if(vertex_buffer[i] < model->bb_min.x) model->bb_min.x = vertex_buffer[i];
			if(vertex_buffer[i+1] < model->bb_min.y) model->bb_min.y = vertex_buffer[i+1];
			if(vertex_buffer[i+2] < model->bb_min.z) model->bb_min.z = vertex_buffer[i+2];

			if(vertex_buffer[i] > model->bb_max.x) model->bb_max.x = vertex_buffer[i];
			if(vertex_buffer[i+1] > model->bb_max.y) model->bb_max.y = vertex_buffer[i+1];
			if(vertex_buffer[i+2] > model->bb_max.z) model->bb_max.z = vertex_buffer[i+2];
		}

		// set entity model
		elf_set_entity_model(entity, model);

		elf_generate_entity_tangents(entity);
		elf_add_entity_to_scene(scene, entity);
	}
	else
	{
		elf_destroy_model(model);
		elf_destroy_entity(entity);
	}

	for (i = 0; i < (int)aind->mNumChildren; i++)
	{
		elf_recursively_import_assets(scene, aiscn, aind->mChildren[i]);
	}
}

elf_scene* elf_create_scene_from_file(const char *file_path)
{
	elf_pak *pak;
	elf_scene *scene;
	const char *type;
	int i;
	elf_camera *camera;
	elf_light *light;

	type = strrchr(file_path, '.');
	if(!type)
	{
		elf_set_error(ELF_CANT_OPEN_FILE, "error: can't open file \"%s\"\n", file_path);
		return NULL;
	}

	if(!strcmp(type, ".pak"))
	{
		pak = elf_create_pak_from_file(file_path);
		if(!pak) return NULL;

		scene = elf_create_scene_from_pak(pak);

		return scene;
	}
	else if(aiIsExtensionSupported(type))
	{
		const struct aiScene *aiscn;
		struct aiLogStream stream;

		stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT,NULL);
		aiAttachLogStream(&stream);

		aiscn = aiImportFile(file_path, aiProcessPreset_TargetRealtime_Quality);
		if(!aiscn)
		{
			aiDetachAllLogStreams();
			return NULL;
		}

		scene = elf_create_scene(file_path);
		scene->file_path = elf_create_string(file_path);

		elf_recursively_import_assets(scene, aiscn, aiscn->mRootNode);

		for(i = 0; i < aiscn->mNumCameras; i++)
		{
			const struct aiCamera *aicam = aiscn->mCameras[i];

			camera = elf_create_camera(aicam->mName.data);

			elf_set_camera_perspective(camera, aicam->mHorizontalFOV, -1.0, aicam->mClipPlaneNear, aicam->mClipPlaneFar);
			elf_set_actor_position((elf_actor*)camera, aicam->mPosition.x, aicam->mPosition.y, aicam->mPosition.z);

			elf_add_camera_to_scene(scene, camera);
		}

		for(i = 0; i < aiscn->mNumLights; i++)
		{
			const struct aiLight *ailig = aiscn->mLights[i];

			light = elf_create_light(ailig->mName.data);

			if(ailig->mType == aiLightSource_DIRECTIONAL) elf_set_light_type(light, ELF_SUN_LIGHT);
			else if(ailig->mType == aiLightSource_POINT) elf_set_light_type(light, ELF_POINT_LIGHT);
			else if(ailig->mType == aiLightSource_SPOT) elf_set_light_type(light, ELF_SPOT_LIGHT);

			elf_set_light_color(light, ailig->mColorDiffuse.r, ailig->mColorDiffuse.g, ailig->mColorDiffuse.b, 1.0);
			elf_set_light_cone(light, ailig->mAngleInnerCone, ailig->mAngleOuterCone);
			elf_set_light_distance(light, 0.0001);
			elf_set_light_fade_speed(light, (ailig->mAttenuationLinear+ailig->mAttenuationQuadratic)/2.0);
			elf_set_actor_position((elf_actor*)light, ailig->mPosition.x, ailig->mPosition.y, ailig->mPosition.z);
			elf_set_actor_direction((elf_actor*)light, ailig->mDirection.x, ailig->mDirection.y, ailig->mDirection.z);

			elf_add_light_to_scene(scene, light);
		}

		aiReleaseImport(aiscn);
		aiDetachAllLogStreams();

		return scene;
	}
	else
	{
		elf_set_error(ELF_INVALID_FILE, "error: can't open \"%s\", unsupported format\n", file_path);
		return NULL;
	}
}

unsigned char elf_save_scene(elf_scene *scene, const char *file_path)
{
	return elf_save_scene_to_pak(scene, file_path);
}

void elf_update_scene(elf_scene *scene, float sync)
{
	elf_camera *cam;
	elf_entity *ent;
	elf_light *light;
	elf_particles *par;
	elf_sprite *spr;
	float position[3];
	float orient[4];
	float vec_z[3] = {0.0, 0.0, -1.0};
	float vec_y[3] = {0.0, 1.0, -1.0};
	float front_up_vec[6];

	if(sync > 0.0)
	{
		if(scene->physics) elf_update_physics_world(scene->world, sync);
		elf_update_physics_world(scene->dworld, sync);
	}

	if(scene->cur_camera)
	{
		elf_get_actor_position_((elf_actor*)scene->cur_camera, position);
		elf_get_actor_orientation_((elf_actor*)scene->cur_camera, orient);
		gfx_mul_qua_vec(orient, vec_z, &front_up_vec[0]);
		gfx_mul_qua_vec(orient, vec_y, &front_up_vec[3]);
		elf_set_audio_listener_position(position[0], position[1], position[2]);
		elf_set_audio_listener_orientation(front_up_vec);
	}

	// logics update pass
	for(cam = (elf_camera*)elf_begin_list(scene->cameras); cam != NULL;
		cam = (elf_camera*)elf_next_in_list(scene->cameras))
	{
		elf_update_camera(cam);
	}

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		elf_update_entity(ent);
	}

	for(light = (elf_light*)elf_begin_list(scene->lights); light != NULL;
		light = (elf_light*)elf_next_in_list(scene->lights))
	{
		elf_update_light(light);
	}

	for(par = (elf_particles*)elf_begin_list(scene->particles); par != NULL;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		elf_update_particles(par, sync);
	}

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		elf_update_sprite(spr);
	}
}

void elf_scene_pre_draw(elf_scene *scene)
{
	elf_camera *cam;
	elf_entity *ent;
	elf_light *light;
	elf_sprite *spr;
	elf_particles *par;

	for(cam = (elf_camera*)elf_begin_list(scene->cameras); cam != NULL;
		cam = (elf_camera*)elf_next_in_list(scene->cameras))
	{
		elf_camera_pre_draw(cam);
	}

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		elf_entity_pre_draw(ent);
	}

	for(light = (elf_light*)elf_begin_list(scene->lights); light != NULL;
		light = (elf_light*)elf_next_in_list(scene->lights))
	{
		elf_light_pre_draw(light);
	}

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		elf_sprite_pre_draw(spr, scene->cur_camera);
	}

	for(par = (elf_particles*)elf_begin_list(scene->particles); par != NULL;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		elf_particles_pre_draw(par);
	}
}

void elf_scene_post_draw(elf_scene *scene)
{
	elf_camera *cam;
	elf_entity *ent;
	elf_light *light;
	elf_sprite *spr;
	elf_particles *par;

	for(cam = (elf_camera*)elf_begin_list(scene->cameras); cam != NULL;
		cam = (elf_camera*)elf_next_in_list(scene->cameras))
	{
		elf_camera_post_draw(cam);
	}

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		elf_entity_post_draw(ent);
	}

	for(light = (elf_light*)elf_begin_list(scene->lights); light != NULL;
		light = (elf_light*)elf_next_in_list(scene->lights))
	{
		elf_light_post_draw(light);
	}

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		elf_sprite_post_draw(spr);
	}

	for(par = (elf_particles*)elf_begin_list(scene->particles); par != NULL;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		elf_particles_post_draw(par);
	}
}

void elf_destroy_scene(void *data)
{
	elf_scene *scene = (elf_scene*)data;

	elf_actor *actor;

	if(scene->name) elf_destroy_string(scene->name);
	if(scene->file_path) elf_destroy_string(scene->file_path);

	if(scene->entity_queue) elf_dec_ref((elf_object*)scene->entity_queue);
	if(scene->sprite_queue) elf_dec_ref((elf_object*)scene->sprite_queue);

	for(actor = (elf_actor*)elf_begin_list(scene->cameras); actor;
		actor = (elf_actor*)elf_next_in_list(scene->cameras)) elf_remove_actor(actor);
	for(actor = (elf_actor*)elf_begin_list(scene->entities); actor;
		actor = (elf_actor*)elf_next_in_list(scene->entities)) elf_remove_actor(actor);
	for(actor = (elf_actor*)elf_begin_list(scene->lights); actor;
		actor = (elf_actor*)elf_next_in_list(scene->lights)) elf_remove_actor(actor);
	for(actor = (elf_actor*)elf_begin_list(scene->particles); actor;
		actor = (elf_actor*)elf_next_in_list(scene->particles)) elf_remove_actor(actor);
	for(actor = (elf_actor*)elf_begin_list(scene->sprites); actor;
		actor = (elf_actor*)elf_next_in_list(scene->sprites)) elf_remove_actor(actor);

	if(scene->models) elf_dec_ref((elf_object*)scene->models);
	if(scene->scripts) elf_dec_ref((elf_object*)scene->scripts);
	if(scene->materials) elf_dec_ref((elf_object*)scene->materials);
	if(scene->textures) elf_dec_ref((elf_object*)scene->textures);
	if(scene->cameras) elf_dec_ref((elf_object*)scene->cameras);
	if(scene->entities) elf_dec_ref((elf_object*)scene->entities);
	if(scene->lights) elf_dec_ref((elf_object*)scene->lights);
	if(scene->armatures) elf_dec_ref((elf_object*)scene->armatures);
	if(scene->particles) elf_dec_ref((elf_object*)scene->particles);
	if(scene->sprites) elf_dec_ref((elf_object*)scene->sprites);

	elf_destroy_physics_world(scene->world);
	elf_destroy_physics_world(scene->dworld);

	if(scene->pak) elf_dec_ref((elf_object*)scene->pak);

	if(scene->compose_main_shdr) gfx_destroy_shader_program(scene->compose_main_shdr);

	elf_dec_obj(ELF_SCENE);

	free(scene);
}

void elf_set_scene_ambient_color(elf_scene *scene, float r, float g, float b, float a)
{
	scene->ambient_color.r = r;
	scene->ambient_color.g = g;
	scene->ambient_color.b = b;
	scene->ambient_color.a = a;
}

elf_color elf_get_scene_ambient_color(elf_scene *scene)
{
	return scene->ambient_color;
}

void elf_set_scene_gravity(elf_scene *scene, float x, float y, float z)
{
	elf_set_physics_world_gravity(scene->world, x, y, z);
}

elf_vec3f elf_get_scene_gravity(elf_scene *scene)
{
	return elf_get_physics_world_gravity(scene->world);
}

void elf_set_scene_physics(elf_scene *scene, unsigned char physics)
{
	scene->physics = !physics == ELF_FALSE;
}

unsigned char elf_get_scene_physics(elf_scene *scene)
{
	return scene->physics;
}

void elf_set_scene_run_scripts(elf_scene *scene, unsigned char run_scripts)
{
	scene->run_scripts = !run_scripts == ELF_FALSE;
}

unsigned char elf_get_scene_run_scripts(elf_scene *scene, unsigned char run_scripts)
{
	return scene->run_scripts;
}

const char* elf_get_scene_name(elf_scene *scene)
{
	return scene->name;
}

const char* elf_get_scene_file_path(elf_scene *scene)
{
	return scene->file_path;
}

int elf_get_scene_camera_count(elf_scene *scene)
{
	return elf_get_list_length(scene->cameras);
}

int elf_get_scene_entity_count(elf_scene *scene)
{
	return elf_get_list_length(scene->entities);
}

int elf_get_scene_light_count(elf_scene *scene)
{
	return elf_get_list_length(scene->lights);
}

int elf_get_scene_armature_count(elf_scene *scene)
{
	return elf_get_list_length(scene->armatures);
}

int elf_get_scene_particles_count(elf_scene *scene)
{
	return elf_get_list_length(scene->particles);
}

int elf_get_scene_sprite_count(elf_scene *scene)
{
	return elf_get_list_length(scene->sprites);
}

void elf_set_actor_scene(elf_scene *scene, elf_actor *actor)
{
	elf_joint *joint;

	if(actor->scene) elf_remove_actor_by_object(actor->scene, actor);

	actor->scene = scene;

	if(actor->object) elf_set_physics_object_world(actor->object, scene->world);
	if(actor->dobject) elf_set_physics_object_world(actor->dobject, scene->dworld);

	for(joint = (elf_joint*)elf_begin_list(actor->joints); joint;
		joint = (elf_joint*)elf_next_in_list(actor->joints))
	{
		elf_set_joint_world(joint, scene->world);
	}
}

void elf_add_camera_to_scene(elf_scene *scene, elf_camera *camera)
{
	if(!camera) return;
	elf_set_actor_scene(scene, (elf_actor*)camera);
	elf_append_to_list(scene->cameras, (elf_object*)camera);
	if(elf_get_list_length(scene->cameras) == 1) scene->cur_camera = camera;
}

void elf_add_entity_to_scene(elf_scene *scene, elf_entity *entity)
{
	if(!entity) return;
	elf_set_actor_scene(scene, (elf_actor*)entity);
	elf_append_to_list(scene->entities, (elf_object*)entity);
}

void elf_add_light_to_scene(elf_scene *scene, elf_light *light)
{
	if(!light) return;
	elf_set_actor_scene(scene, (elf_actor*)light);
	elf_append_to_list(scene->lights, (elf_object*)light);
}

void elf_add_particles_to_scene(elf_scene *scene, elf_particles *particles)
{
	if(!particles) return;
	elf_set_actor_scene(scene, (elf_actor*)particles);
	elf_append_to_list(scene->particles, (elf_object*)particles);
}

void elf_add_sprite_to_scene(elf_scene *scene, elf_sprite *sprite)
{
	if(!sprite) return;
	elf_set_actor_scene(scene, (elf_actor*)sprite);
	elf_append_to_list(scene->sprites, (elf_object*)sprite);
}

void elf_set_scene_active_camera(elf_scene *scene, elf_camera *camera)
{
	scene->cur_camera = camera;
}

elf_camera* elf_get_scene_active_camera(elf_scene *scene)
{
	return scene->cur_camera;
}

elf_collision* elf_get_scene_ray_cast_result(elf_scene *scene, float x, float y, float z, float dx, float dy, float dz)
{
	return elf_get_ray_cast_result(scene->world, x, y, z, dx, dy, dz);
}

elf_list* elf_get_scene_ray_cast_results(elf_scene *scene, float x, float y, float z, float dx, float dy, float dz)
{
	return elf_get_ray_cast_results(scene->world, x, y, z, dx, dy, dz);
}

elf_collision* elf_get_debug_scene_ray_cast_result(elf_scene *scene, float x, float y, float z, float dx, float dy, float dz)
{
	return elf_get_ray_cast_result(scene->dworld, x, y, z, dx, dy, dz);
}

elf_list* elf_get_debug_scene_ray_cast_results(elf_scene *scene, float x, float y, float z, float dx, float dy, float dz)
{
	return elf_get_ray_cast_results(scene->dworld, x, y, z, dx, dy, dz);
}

elf_camera* elf_get_camera_by_index(elf_scene *scene, int idx)
{
	return (elf_camera*)elf_get_item_from_list(scene->cameras, idx);
}

elf_entity* elf_get_entity_by_index(elf_scene *scene, int idx)
{
	return (elf_entity*)elf_get_item_from_list(scene->entities, idx);
}

elf_light* elf_get_light_by_index(elf_scene *scene, int idx)
{
	return (elf_light*)elf_get_item_from_list(scene->lights, idx);
}

elf_armature* elf_get_armature_by_index(elf_scene *scene, int idx)
{
	return (elf_armature*)elf_get_item_from_list(scene->armatures, idx);
}

elf_particles* elf_get_particles_by_index(elf_scene *scene, int idx)
{
	return (elf_particles*)elf_get_item_from_list(scene->particles, idx);
}

elf_sprite* elf_get_sprite_by_index(elf_scene *scene, int idx)
{
	return (elf_sprite*)elf_get_item_from_list(scene->sprites, idx);
}

elf_texture *elf_get_texture_by_name(elf_scene *scene, const char *name)
{
	elf_texture *texture;

	for(texture = (elf_texture*)elf_begin_list(scene->textures); texture;
		texture = (elf_texture*)elf_next_in_list(scene->textures))
	{
		if(!strcmp(texture->name, name)) return texture;
	}

	return NULL;
}

elf_model *elf_get_model_by_name(elf_scene *scene, const char *name)
{
	elf_model *model;

	for(model = (elf_model*)elf_begin_list(scene->models); model;
		model = (elf_model*)elf_next_in_list(scene->models))
	{
		if(!strcmp(model->name, name)) return model;
	}

	return NULL;
}

elf_script *elf_get_script_by_name(elf_scene *scene, const char *name)
{
	elf_script *script;

	for(script = (elf_script*)elf_begin_list(scene->scripts); script;
		script = (elf_script*)elf_next_in_list(scene->scripts))
	{
		if(!strcmp(script->name, name)) return script;
	}

	return NULL;
}

elf_material *elf_get_material_by_name(elf_scene *scene, const char *name)
{
	elf_material *material;

	for(material = (elf_material*)elf_begin_list(scene->materials); material;
		material = (elf_material*)elf_next_in_list(scene->materials))
	{
		if(!strcmp(material->name, name)) return material;
	}

	return NULL;
}

elf_actor *elf_get_actor_by_name(elf_scene *scene, const char *name)
{
	elf_actor *actor;

	actor = (elf_actor*)elf_get_camera_by_name(scene, name);
	if(actor) return actor;
	actor = (elf_actor*)elf_get_entity_by_name(scene, name);
	if(actor) return actor;
	actor = (elf_actor*)elf_get_light_by_name(scene, name);
	if(actor) return actor;
	actor = (elf_actor*)elf_get_armature_by_name(scene, name);
	if(actor) return actor;
	actor = (elf_actor*)elf_get_particles_by_name(scene, name);
	if(actor) return actor;

	return NULL;
}

elf_camera *elf_get_camera_by_name(elf_scene *scene, const char *name)
{
	elf_camera *camera;

	for(camera = (elf_camera*)elf_begin_list(scene->cameras); camera;
		camera = (elf_camera*)elf_next_in_list(scene->cameras))
	{
		if(!strcmp(camera->name, name)) return camera;
	}

	return NULL;
}

elf_entity *elf_get_entity_by_name(elf_scene *scene, const char *name)
{
	elf_entity *entity;

	for(entity = (elf_entity*)elf_begin_list(scene->entities); entity;
		entity = (elf_entity*)elf_next_in_list(scene->entities))
	{
		if(!strcmp(entity->name, name)) return entity;
	}

	return NULL;
}

elf_light *elf_get_light_by_name(elf_scene *scene, const char *name)
{
	elf_light *light;

	for(light = (elf_light*)elf_begin_list(scene->lights); light;
		light = (elf_light*)elf_next_in_list(scene->lights))
	{
		if(!strcmp(light->name, name)) return light;
	}

	return NULL;
}

elf_armature *elf_get_armature_by_name(elf_scene *scene, const char *name)
{
	elf_armature *armature;

	for(armature = (elf_armature*)elf_begin_list(scene->armatures); armature;
		armature = (elf_armature*)elf_next_in_list(scene->armatures))
	{
		if(!strcmp(armature->name, name)) return armature;
	}

	return NULL;
}

elf_particles *elf_get_particles_by_name(elf_scene *scene, const char *name)
{
	elf_particles *particles;

	for(particles = (elf_particles*)elf_begin_list(scene->particles); particles;
		particles = (elf_particles*)elf_next_in_list(scene->particles))
	{
		if(!strcmp(particles->name, name)) return particles;
	}

	return NULL;
}

elf_sprite *elf_get_sprite_by_name(elf_scene *scene, const char *name)
{
	elf_sprite *sprite;

	for(sprite = (elf_sprite*)elf_begin_list(scene->sprites); sprite;
		sprite = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		if(!strcmp(sprite->name, name)) return sprite;
	}

	return NULL;
}

elf_texture* elf_get_or_load_texture_by_name(elf_scene *scene, const char *name)
{
	elf_texture *texture;
	elf_pak_index *index;
	FILE *file;

	for(texture = (elf_texture*)elf_begin_list(scene->textures); texture;
		texture = (elf_texture*)elf_next_in_list(scene->textures))
	{
		if(!strcmp(texture->name, name)) return texture;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_TEXTURE);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			texture = elf_create_texture_from_pak(file, name, scene);
			if(texture) elf_append_to_list(scene->textures, (elf_object*)texture);
			fclose(file);
			return texture;
		}
	}

	return NULL;
}

elf_model* elf_get_or_load_model_by_name(elf_scene *scene, const char *name)
{
	elf_model *model;
	elf_pak_index *index;
	FILE *file;

	for(model = (elf_model*)elf_begin_list(scene->models); model;
		model = (elf_model*)elf_next_in_list(scene->models))
	{
		if(!strcmp(model->name, name)) return model;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_MODEL);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			model = elf_create_model_from_pak(file, name, scene);
			if(model) elf_append_to_list(scene->models, (elf_object*)model);
			fclose(file);
			return model;
		}
	}

	return NULL;
}

elf_script* elf_get_or_load_script_by_name(elf_scene *scene, const char *name)
{
	elf_script *script;
	elf_pak_index *index;
	FILE *file;

	for(script = (elf_script*)elf_begin_list(scene->scripts); script;
		script = (elf_script*)elf_next_in_list(scene->scripts))
	{
		if(!strcmp(script->name, name)) return script;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_SCRIPT);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			script = elf_create_script_from_pak(file, name, scene);
			if(script) elf_append_to_list(scene->scripts, (elf_object*)script);
			fclose(file);
			return script;
		}
	}

	return NULL;
}

elf_material *elf_get_or_load_material_by_name(elf_scene *scene, const char *name)
{
	elf_material *material;
	elf_pak_index *index;
	FILE *file;

	for(material = (elf_material*)elf_begin_list(scene->materials); material;
		material = (elf_material*)elf_next_in_list(scene->materials))
	{
		if(!strcmp(material->name, name)) return material;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_MATERIAL);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			material = elf_create_material_from_pak(file, name, scene);
			if(material) elf_append_to_list(scene->materials, (elf_object*)material);
			fclose(file);
			return material;
		}
	}

	return NULL;
}

elf_camera *elf_get_or_load_camera_by_name(elf_scene *scene, const char *name)
{
	elf_camera *camera;
	elf_pak_index *index;
	FILE *file;

	for(camera = (elf_camera*)elf_begin_list(scene->cameras); camera;
		camera = (elf_camera*)elf_next_in_list(scene->cameras))
	{
		if(!strcmp(camera->name, name)) return camera;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_CAMERA);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;
			camera = elf_create_camera_from_pak(file, name, scene);
			if(camera) elf_add_camera_to_scene(scene, camera);
			fclose(file);
			return camera;
		}
	}

	return NULL;
}

elf_entity *elf_get_or_load_entity_by_name(elf_scene *scene, const char *name)
{
	elf_entity *entity;
	elf_pak_index *index;
	FILE *file;

	for(entity = (elf_entity*)elf_begin_list(scene->entities); entity;
		entity = (elf_entity*)elf_next_in_list(scene->entities))
	{
		if(!strcmp(entity->name, name)) return entity;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_ENTITY);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			entity = elf_create_entity_from_pak(file, name, scene);
			if(entity) elf_add_entity_to_scene(scene, entity);
			fclose(file);
			return entity;
		}
	}

	return NULL;
}

elf_light *elf_get_or_load_light_by_name(elf_scene *scene, const char *name)
{
	elf_light *light;
	elf_pak_index *index;
	FILE *file;

	for(light = (elf_light*)elf_begin_list(scene->lights); light;
		light = (elf_light*)elf_next_in_list(scene->lights))
	{
		if(!strcmp(light->name, name)) return light;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_LIGHT);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			light = elf_create_light_from_pak(file, name, scene);
			if(light) elf_add_light_to_scene(scene, light);
			fclose(file);
			return light;
		}
	}

	return NULL;
}

elf_armature *elf_get_or_load_armature_by_name(elf_scene *scene, const char *name)
{
	elf_armature *armature;
	elf_pak_index *index;
	FILE *file;

	for(armature = (elf_armature*)elf_begin_list(scene->armatures); armature;
		armature = (elf_armature*)elf_next_in_list(scene->armatures))
	{
		if(!strcmp(armature->name, name)) return armature;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_ARMATURE);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			armature = elf_create_armature_from_pak(file, name, scene);
			elf_append_to_list(scene->armatures, (elf_object*)armature);
			fclose(file);
			return armature;
		}
	}

	return NULL;
}

elf_particles *elf_get_or_load_particles_by_name(elf_scene *scene, const char *name)
{
	elf_particles *particles;
	elf_pak_index *index;
	FILE *file;

	for(particles = (elf_particles*)elf_begin_list(scene->particles); particles;
		particles = (elf_particles*)elf_next_in_list(scene->particles))
	{
		if(!strcmp(particles->name, name)) return particles;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_PARTICLES);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			particles = elf_create_particles_from_pak(file, name, scene);
			if(particles) elf_add_particles_to_scene(scene, particles);
			fclose(file);
			return particles;
		}
	}

	return NULL;
}

elf_sprite *elf_get_or_load_sprite_by_name(elf_scene *scene, const char *name)
{
	elf_sprite *sprite;
	elf_pak_index *index;
	FILE *file;

	for(sprite = (elf_sprite*)elf_begin_list(scene->sprites); sprite;
		sprite = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		if(!strcmp(sprite->name, name)) return sprite;
	}

	if(scene->pak)
	{
		index = elf_get_pak_index_by_name(scene->pak, name, ELF_SPRITE);
		if(index)
		{
			file = fopen(scene->pak->file_path, "rb");
			fseek(file, elf_get_pak_index_offset(index), SEEK_SET);
			if(feof(file)) return NULL;

			sprite = elf_create_sprite_from_pak(file, name, scene);
			if(sprite) elf_add_sprite_to_scene(scene, sprite);
			fclose(file);
			return sprite;
		}
	}

	return NULL;
}

elf_actor *elf_get_or_load_actor_by_name(elf_scene *scene, const char *name)
{
	elf_actor *actor;

	actor = (elf_actor*)elf_get_or_load_camera_by_name(scene, name);
	if(actor) return actor;
	actor = (elf_actor*)elf_get_or_load_entity_by_name(scene, name);
	if(actor) return actor;
	actor = (elf_actor*)elf_get_or_load_light_by_name(scene, name);
	if(actor) return actor;
	actor = (elf_actor*)elf_get_or_load_particles_by_name(scene, name);
	if(actor) return actor;

	return NULL;
}

void elf_remove_actor(elf_actor *actor)
{
	elf_joint *joint;

	actor->scene = NULL;

	if(actor->object)
	{
		elf_remove_physics_object_collisions(actor->object);
		elf_set_physics_object_world(actor->object, NULL);
	}
	if(actor->dobject)
	{
		elf_remove_physics_object_collisions(actor->dobject);
		elf_set_physics_object_world(actor->dobject, NULL);
	}

	for(joint = (elf_joint*)elf_begin_list(actor->joints); joint;
		joint = (elf_joint*)elf_next_in_list(actor->joints))
	{
		elf_set_joint_world(joint, NULL);
	}

	elf_dec_ref((elf_object*)actor->joints);
	actor->joints = elf_create_list();
	elf_inc_ref((elf_object*)actor->joints);
}

unsigned char elf_remove_camera_by_name(elf_scene *scene, const char *name)
{
	elf_camera *cam;

	for(cam = (elf_camera*)elf_begin_list(scene->cameras); cam != NULL;
		cam = (elf_camera*)elf_next_in_list(scene->cameras))
	{
		if(!strcmp(cam->name, name))
		{
			elf_remove_actor((elf_actor*)cam);
			elf_remove_from_list(scene->cameras, (elf_object*)cam);
			if(scene->cur_camera == cam) scene->cur_camera = elf_get_camera_by_index(scene, 0);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_entity_by_name(elf_scene *scene, const char *name)
{
	elf_entity *ent;

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		if(!strcmp(ent->name, name))
		{
			elf_remove_actor((elf_actor*)ent);
			elf_remove_from_list(scene->entities, (elf_object*)ent);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_light_by_name(elf_scene *scene, const char *name)
{
	elf_light *lig;

	for(lig = (elf_light*)elf_begin_list(scene->lights); lig != NULL;
		lig = (elf_light*)elf_next_in_list(scene->lights))
	{
		if(!strcmp(lig->name, name))
		{
			elf_remove_actor((elf_actor*)lig);
			elf_remove_from_list(scene->lights, (elf_object*)lig);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_particles_by_name(elf_scene *scene, const char *name)
{
	elf_particles *par;

	for(par = (elf_particles*)elf_begin_list(scene->particles); par != NULL;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		if(!strcmp(par->name, name))
		{
			elf_remove_actor((elf_actor*)par);
			elf_remove_from_list(scene->particles, (elf_object*)par);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_sprite_by_name(elf_scene *scene, const char *name)
{
	elf_sprite *spr;

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		if(!strcmp(spr->name, name))
		{
			elf_remove_actor((elf_actor*)spr);
			elf_remove_from_list(scene->sprites, (elf_object*)spr);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_camera_by_index(elf_scene *scene, int idx)
{
	elf_camera *cam;
	int i;

	if(idx < 0 || idx > elf_get_list_length(scene->cameras)-1) return ELF_FALSE;

	for(i = 0, cam = (elf_camera*)elf_begin_list(scene->cameras); cam != NULL;
		cam = (elf_camera*)elf_next_in_list(scene->cameras), i++)
	{
		if(i == idx)
		{
			elf_remove_actor((elf_actor*)cam);
			elf_remove_from_list(scene->cameras, (elf_object*)cam);
			if(scene->cur_camera == cam)
			{
				scene->cur_camera = elf_get_camera_by_index(scene, 0);
			}
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_entity_by_index(elf_scene *scene, int idx)
{
	elf_entity *ent;
	int i;

	if(idx < 0 || idx > elf_get_list_length(scene->entities)-1) return ELF_FALSE;

	for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
		ent = (elf_entity*)elf_next_in_list(scene->entities), i++)
	{
		if(i == idx)
		{
			elf_remove_actor((elf_actor*)ent);
			elf_remove_from_list(scene->entities, (elf_object*)ent);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_light_by_index(elf_scene *scene, int idx)
{
	elf_light *lig;
	int i;

	if(idx < 0 || idx > elf_get_list_length(scene->lights)-1) return ELF_FALSE;

	for(i = 0, lig = (elf_light*)elf_begin_list(scene->lights); lig != NULL;
		lig = (elf_light*)elf_next_in_list(scene->lights), i++)
	{
		if(i == idx)
		{
			elf_remove_actor((elf_actor*)lig);
			elf_remove_from_list(scene->lights, (elf_object*)lig);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_particles_by_index(elf_scene *scene, int idx)
{
	elf_particles *par;
	int i;

	if(idx < 0 || idx > elf_get_list_length(scene->particles)-1) return ELF_FALSE;

	for(i = 0, par = (elf_particles*)elf_begin_list(scene->particles); par != NULL;
		par = (elf_particles*)elf_next_in_list(scene->particles), i++)
	{
		if(i == idx)
		{
			elf_remove_actor((elf_actor*)par);
			elf_remove_from_list(scene->particles, (elf_object*)par);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_sprite_by_index(elf_scene *scene, int idx)
{
	elf_sprite *spr;
	int i;

	if(idx < 0 || idx > elf_get_list_length(scene->sprites)-1) return ELF_FALSE;

	for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites), i++)
	{
		if(i == idx)
		{
			elf_remove_actor((elf_actor*)spr);
			elf_remove_from_list(scene->sprites, (elf_object*)spr);
			return ELF_TRUE;
		}
	}

	return ELF_FALSE;
}

unsigned char elf_remove_camera_by_object(elf_scene *scene, elf_camera *camera)
{
	elf_remove_actor((elf_actor*)camera);
	if(scene->cur_camera == camera) scene->cur_camera = elf_get_camera_by_index(scene, 0);
	return elf_remove_from_list(scene->cameras, (elf_object*)camera);
}

unsigned char elf_remove_entity_by_object(elf_scene *scene, elf_entity *entity)
{
	elf_remove_actor((elf_actor*)entity);
	return elf_remove_from_list(scene->entities, (elf_object*)entity);
}

unsigned char elf_remove_light_by_object(elf_scene *scene, elf_light *light)
{
	elf_remove_actor((elf_actor*)light);
	return elf_remove_from_list(scene->lights, (elf_object*)light);
}

unsigned char elf_remove_particles_by_object(elf_scene *scene, elf_particles *particles)
{
	elf_remove_actor((elf_actor*)particles);
	return elf_remove_from_list(scene->particles, (elf_object*)particles);
}

unsigned char elf_remove_sprite_by_object(elf_scene *scene, elf_sprite *sprite)
{
	elf_remove_actor((elf_actor*)sprite);
	return elf_remove_from_list(scene->sprites, (elf_object*)sprite);
}

unsigned char elf_remove_actor_by_object(elf_scene *scene, elf_actor *actor)
{
	unsigned char result;

	result = elf_remove_camera_by_object(scene, (elf_camera*)actor);
	if(result) return result;
	result = elf_remove_entity_by_object(scene, (elf_entity*)actor);
	if(result) return result;
	result = elf_remove_light_by_object(scene, (elf_light*)actor);
	if(result) return result;
	result = elf_remove_particles_by_object(scene, (elf_particles*)actor);
	if(result) return result;
	result = elf_remove_sprite_by_object(scene, (elf_sprite*)actor);
	if(result) return result;

	return ELF_FALSE;
}

void elf_draw_scene(elf_scene *scene)
{
	elf_light *light;
	elf_entity *ent;
	elf_sprite *spr;
	elf_particles *par;
	float bias[16] = {0.5, 0.0, 0.0, 0.0,
			0.0, 0.5, 0.0, 0.0,
			0.0, 0.0, 0.5, 0.0,
			0.5, 0.5, 0.5, 1.0};
	float temp_mat1[16];
	float temp_mat2[16];
	gfx_render_target *render_target;
	int i;
	elf_vec3f lpos;
	elf_vec3f epos;
	elf_vec3f eofs;
	elf_vec4f eori;
	elf_vec3f spos;
	elf_vec3f dvec;
	float dist, att;
	unsigned char found;

	if(!scene->cur_camera) return;

	render_target = gfx_get_cur_render_target();

	// draw depth buffer
	gfx_set_shader_params_default(&scene->shader_params);
	elf_set_camera(scene->cur_camera, &scene->shader_params);
	scene->shader_params.render_params.color_write = ELF_FALSE;
	scene->shader_params.render_params.alpha_write = ELF_FALSE;

	scene->entity_queue_count = 0;
	elf_begin_list(scene->entity_queue);

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		if(!elf_cull_entity(ent, scene->cur_camera))
		{
			if(scene->entity_queue_count < elf_get_list_length(scene->entity_queue))
			{
				elf_set_list_cur_ptr(scene->entity_queue, (elf_object*)ent);
				elf_next_in_list(scene->entity_queue);
			}
			else
			{
				elf_append_to_list(scene->entity_queue, (elf_object*)ent);
			}
			scene->entity_queue_count++;
			elf_draw_entity(ent, ELF_DRAW_DEPTH, &scene->shader_params);
			ent->culled = ELF_FALSE;
		}
		else
		{
			ent->culled = ELF_TRUE;
		}
	}

	scene->sprite_queue_count = 0;
	elf_begin_list(scene->sprite_queue);

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		if(!elf_cull_sprite(spr, scene->cur_camera))
		{
			if(scene->sprite_queue_count < elf_get_list_length(scene->sprite_queue))
			{
				elf_set_list_cur_ptr(scene->sprite_queue, (elf_object*)spr);
				elf_next_in_list(scene->sprite_queue);
			}
			else
			{
				elf_append_to_list(scene->sprite_queue, (elf_object*)spr);
			}
			scene->sprite_queue_count++;
			elf_draw_sprite(spr, ELF_DRAW_DEPTH, &scene->shader_params);
			spr->culled = ELF_FALSE;
		}
		else
		{
			spr->culled = ELF_TRUE;
		}
	}

	// initiate occlusion queries
	if(eng->occlusion_culling)
	{
		gfx_set_shader_params_default(&scene->shader_params);
		elf_set_camera(scene->cur_camera, &scene->shader_params);
		scene->shader_params.render_params.depth_write = GFX_FALSE;
		scene->shader_params.render_params.depth_func = GFX_LEQUAL;
		scene->shader_params.render_params.color_write = GFX_FALSE;
		scene->shader_params.render_params.alpha_write = GFX_FALSE;
		scene->shader_params.render_params.cull_face = GFX_FALSE;
		scene->shader_params.render_params.offset_bias = -1.0;
		scene->shader_params.render_params.offset_scale = -1.0;

		for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
			i < scene->entity_queue_count && ent != NULL;
			i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
		{
			gfx_begin_query(ent->query);
			elf_draw_entity_bounding_box(ent, &scene->shader_params);
			gfx_end_query(ent->query);
		}

		scene->shader_params.render_params.offset_bias = 0.0;
		scene->shader_params.render_params.offset_scale = 0.0;
	}

	eng->ambient_color = scene->ambient_color;

	// draw ambient pass
	if(!elf_about_zero(eng->ambient_color.r) ||
		!elf_about_zero(eng->ambient_color.g) ||
		!elf_about_zero(eng->ambient_color.b) )
	{
		gfx_set_shader_params_default(&scene->shader_params);
		elf_set_camera(scene->cur_camera, &scene->shader_params);

		scene->shader_params.render_params.depth_write = GFX_FALSE;
		scene->shader_params.render_params.depth_func = GFX_EQUAL;
		scene->shader_params.render_params.blend_mode = GFX_ADD;

		for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
			i < scene->entity_queue_count && ent != NULL;
			i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
		{
			elf_draw_entity(ent, ELF_DRAW_AMBIENT, &scene->shader_params);
		}

		for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
			i < scene->sprite_queue_count && spr != NULL;
			i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
		{
			elf_draw_sprite(spr, ELF_DRAW_AMBIENT, &scene->shader_params);
		}
	}

	// draw non lighted stuff
	gfx_set_shader_params_default(&scene->shader_params);
	elf_set_camera(scene->cur_camera, &scene->shader_params);

	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_func = GFX_EQUAL;
	scene->shader_params.render_params.blend_mode = GFX_ADD;

	for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
		i < scene->entity_queue_count && ent != NULL;
		i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
	{
		elf_draw_entity(ent, ELF_DRAW_WITHOUT_LIGHTING, &scene->shader_params);
	}

	for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
		i < scene->sprite_queue_count && spr != NULL;
		i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
	{
		elf_draw_sprite(spr, ELF_DRAW_WITHOUT_LIGHTING, &scene->shader_params);
	}

	// render lighting
	for(light = (elf_light*)elf_begin_list(scene->lights); light != NULL;
		light = (elf_light*)elf_next_in_list(scene->lights))
	{
		if(!light->visible) continue;

		// need to call this for frustum culling...
		if(light->light_type == ELF_SPOT_LIGHT)
		{
			elf_set_camera_viewport(light->shadow_camera, 0, 0, elf_get_shadow_map_size(), elf_get_shadow_map_size());
			elf_set_camera(light->shadow_camera, &scene->shader_params);

			// check are there any entities visible for the spot, if there aren't don't bother continuing, just skip to the next light
			found = ELF_FALSE;
			for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
				i < scene->entity_queue_count && ent != NULL;
				i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
			{
				if(!elf_cull_entity(ent, light->shadow_camera))
				{
					found = ELF_TRUE;
					break;
				}
			}

			for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
				i < scene->sprite_queue_count && spr != NULL;
				i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
			{
				if(!elf_cull_sprite(spr, light->shadow_camera))
				{
					found = ELF_TRUE;
					break;
				}
			}

			if(!found) continue;
		}

		// render shadow map if needed
		if(light->light_type == ELF_SPOT_LIGHT && light->shadows)
		{
			gfx_set_shader_params_default(&scene->shader_params);
			scene->shader_params.render_params.color_write = GFX_FALSE;
			scene->shader_params.render_params.alpha_write = GFX_FALSE;
			scene->shader_params.render_params.offset_bias = 2.0;
			scene->shader_params.render_params.offset_scale = 4.0;
			elf_set_camera(light->shadow_camera, &scene->shader_params);
			gfx_set_shader_params(&scene->shader_params);

			gfx_set_render_target(eng->shadow_target);
			gfx_clear_depth_buffer(1.0);

			for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
				ent = (elf_entity*)elf_next_in_list(scene->entities))
			{
				if(!elf_cull_entity(ent, light->shadow_camera))
				{
					elf_draw_entity(ent, ELF_DRAW_DEPTH, &scene->shader_params);
				}
			}

			for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
				spr = (elf_sprite*)elf_next_in_list(scene->sprites))
			{
				if(!elf_cull_sprite(spr, light->shadow_camera))
				{
					elf_draw_sprite(spr, ELF_DRAW_DEPTH, &scene->shader_params);
				}
			}

			gfx_mul_matrix4_matrix4(elf_get_camera_projection_matrix(light->shadow_camera), bias, temp_mat1);
			gfx_mul_matrix4_matrix4(elf_get_camera_modelview_matrix(light->shadow_camera), temp_mat1, temp_mat2);
			gfx_matrix4_get_inverse_fast(elf_get_camera_modelview_matrix(scene->cur_camera), temp_mat1);
			gfx_mul_matrix4_matrix4(temp_mat1, temp_mat2, light->projection_matrix);

			if(render_target) gfx_set_render_target(render_target);
			else gfx_disable_render_target();
		}

		// render lighting
		gfx_set_shader_params_default(&scene->shader_params);
		scene->shader_params.render_params.depth_write = GFX_FALSE;
		scene->shader_params.render_params.depth_func = GFX_EQUAL;
		scene->shader_params.render_params.color_write = GFX_TRUE;
		scene->shader_params.render_params.alpha_write = GFX_TRUE;
		scene->shader_params.render_params.blend_mode = GFX_ADD;
		elf_set_camera(scene->cur_camera, &scene->shader_params);

		if(light->light_type == ELF_SPOT_LIGHT && light->shadows)
		{
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].type = GFX_SHADOW_MAP;
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].texture = eng->shadow_map;
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].projection_mode = GFX_SHADOW_PROJECTION;
			memcpy(scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].matrix,
				light->projection_matrix, sizeof(float)*16);
		}
		else
		{
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].type = GFX_COLOR_MAP;
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].texture = NULL;
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].projection_mode = GFX_NONE;
			gfx_matrix4_set_identity(scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].matrix);
		}

		elf_set_light(light, scene->cur_camera, &scene->shader_params);

		// get the light position for culling point light entities
		lpos = elf_get_actor_position((elf_actor*)light);
		for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
			i < scene->entity_queue_count && ent != NULL;
			i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
		{
			// get the entity position for culling point light entities and testing against bounding sphere
			epos = elf_get_actor_position((elf_actor*)ent);
			eori = elf_get_actor_orientation((elf_actor*)ent);
			eofs = elf_mul_qua_vec3f(eori, ent->bb_offset);
			epos = elf_add_vec3f_vec3f(epos, eofs);

			if((eng->occlusion_culling && gfx_get_query_result(ent->query) > 0) || !eng->occlusion_culling ||
				elf_camera_inside_sphere(scene->cur_camera, &epos.x, ent->cull_radius))
			{
				if(light->light_type == ELF_SPOT_LIGHT)
				{
					if(!elf_cull_entity(ent, light->shadow_camera))
					{
						elf_draw_entity(ent, ELF_DRAW_WITH_LIGHTING, &scene->shader_params);
					}
				}
				else if(light->light_type == ELF_POINT_LIGHT)
				{
					dvec = elf_sub_vec3f_vec3f(epos, lpos);
					dist = elf_get_vec3f_length(dvec);
					dist -= ent->cull_radius;
					att = 1.0-elf_float_max(dist-light->distance, 0.0)*light->fade_speed;
					if(att > 0.0)
					{
						elf_draw_entity(ent, ELF_DRAW_WITH_LIGHTING, &scene->shader_params);
					}
				}
				else
				{
					elf_draw_entity(ent, ELF_DRAW_WITH_LIGHTING, &scene->shader_params);
				}
			}
			else
			{
				ent->culled = ELF_TRUE;
			}
		}

		for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
			i < scene->sprite_queue_count && spr != NULL;
			i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
		{
			spos = elf_get_actor_position((elf_actor*)spr);
			if(light->light_type == ELF_SPOT_LIGHT)
			{
				if(!elf_cull_sprite(spr, light->shadow_camera))
				{
					elf_draw_sprite(spr, ELF_DRAW_WITH_LIGHTING, &scene->shader_params);
				}
			}
			else if(light->light_type == ELF_POINT_LIGHT)
			{
				dvec = elf_sub_vec3f_vec3f(spos, lpos);
				dist = elf_get_vec3f_length(dvec);
				dist -= spr->cull_radius;
				att = 1.0-elf_float_max(dist-light->distance, 0.0)*light->fade_speed;
				if(att > 0.0)
				{
					elf_draw_sprite(spr, ELF_DRAW_WITH_LIGHTING, &scene->shader_params);
				}
			}
			else
			{
				elf_draw_sprite(spr, ELF_DRAW_WITH_LIGHTING, &scene->shader_params);
			}
		}
	}

	// render particles
	gfx_set_shader_params_default(&scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_func = GFX_LEQUAL;
	scene->shader_params.render_params.color_write = GFX_TRUE;
	scene->shader_params.render_params.alpha_write = GFX_TRUE;
	elf_set_camera(scene->cur_camera, &scene->shader_params);
	
	for(par = (elf_particles*)elf_begin_list(scene->particles); par;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		if(!elf_cull_particles(par, scene->cur_camera))
		{
			elf_draw_particles(par, scene->cur_camera, &scene->shader_params);
		}
	}

	// reset state just to be sure...
	gfx_set_shader_params_default(&scene->shader_params);
	gfx_set_shader_params(&scene->shader_params);

	// keep the query lists compact

	if(elf_get_list_length(scene->entity_queue) > scene->entity_queue_count)
	{
		ent = (elf_entity*)elf_rbegin_list(scene->entity_queue);
		if(ent) elf_remove_from_list(scene->entity_queue, (elf_object*)ent);
	}

	if(elf_get_list_length(scene->sprite_queue) > scene->sprite_queue_count)
	{
		spr = (elf_sprite*)elf_rbegin_list(scene->sprite_queue);
		if(spr) elf_remove_from_list(scene->sprite_queue, (elf_object*)spr);
	}
}

/*void elf_draw_scene(elf_scene *scene)
{
	elf_entity *ent;
	elf_sprite *spr;
	elf_light *lig;
	elf_particles *par;
	float bias[16] = {0.5, 0.0, 0.0, 0.0,
			0.0, 0.5, 0.0, 0.0,
			0.0, 0.0, 0.5, 0.0,
			0.5, 0.5, 0.5, 1.0};
	float temp_mat1[16];
	float temp_mat2[16];
	float inv_projection_matrix[16];
	unsigned char found;
	int i;
	elf_vec4f cam_orient;
	elf_vec3f lpos;
	float ldist;
	elf_vec3f ldvec;
	elf_vec3f lsspos;
	elf_vec3f ldsspos;
	elf_vec2f lmin;
	elf_vec2f lmax;
	int wwidth;
	int wheight;

	if(!scene->cur_camera) return;

	gfx_bind_gbuffer(eng->gbuffer, &scene->shader_params);
	gfx_clear_buffers(0.0, 0.0, 0.0, 0.0, 1.0);

	if(eng->occlusion_culling)
	{
		// draw occluders to depth buffer
		gfx_set_shader_params_default(&scene->shader_params);
		elf_set_camera(scene->cur_camera, &scene->shader_params);
		scene->shader_params.render_params.color_write = ELF_FALSE;
		scene->shader_params.render_params.alpha_write = ELF_FALSE;
		scene->shader_params.gbuffer = eng->gbuffer;
		scene->shader_params.gbuffer_mode = GFX_GBUFFER_DEPTH;

		found = ELF_FALSE;
		scene->entity_queue_count = 0;
		elf_begin_list(scene->entity_queue);

		for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
			ent = (elf_entity*)elf_next_in_list(scene->entities))
		{
			if(!elf_cull_entity(ent, scene->cur_camera))
			{
				if(scene->entity_queue_count < elf_get_list_length(scene->entity_queue))
				{
					elf_set_list_cur_ptr(scene->entity_queue, (elf_object*)ent);
					elf_next_in_list(scene->entity_queue);
				}
				else
				{
					elf_append_to_list(scene->entity_queue, (elf_object*)ent);
				}
				scene->entity_queue_count++;
				if(ent->occluder)
				{
					elf_draw_entity(ent, ELF_DRAW_DEPTH, &scene->shader_params);
					found = ELF_TRUE;
				}
				ent->culled = ELF_FALSE;
			}
			else
			{
				ent->culled = ELF_TRUE;
			}
		}

		scene->sprite_queue_count = 0;
		elf_begin_list(scene->sprite_queue);

		for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
			spr = (elf_sprite*)elf_next_in_list(scene->sprites))
		{
			if(!elf_cull_sprite(spr, scene->cur_camera))
			{
				if(scene->sprite_queue_count < elf_get_list_length(scene->sprite_queue))
				{
					elf_set_list_cur_ptr(scene->sprite_queue, (elf_object*)spr);
					elf_next_in_list(scene->sprite_queue);
				}
				else
				{
					elf_append_to_list(scene->sprite_queue, (elf_object*)spr);
				}
				scene->sprite_queue_count++;
				if(spr->occluder)
				{
					found = ELF_TRUE;
					elf_draw_sprite(spr, ELF_DRAW_DEPTH, &scene->shader_params);
				}
				spr->culled = ELF_FALSE;
			}
			else
			{
				spr->culled = ELF_TRUE;
			}
		}

		if(found)
		{
			// initiate occlusion queries
			gfx_set_shader_params_default(&scene->shader_params);
			elf_set_camera(scene->cur_camera, &scene->shader_params);
			scene->shader_params.render_params.depth_write = GFX_FALSE;
			scene->shader_params.render_params.depth_func = GFX_LEQUAL;
			scene->shader_params.render_params.color_write = GFX_FALSE;
			scene->shader_params.render_params.alpha_write = GFX_FALSE;
			scene->shader_params.render_params.cull_face = GFX_FALSE;

			for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
				i < scene->entity_queue_count && ent != NULL;
				i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
			{
				gfx_begin_query(ent->query);
				elf_draw_entity_bounding_box(ent, &scene->shader_params);
				gfx_end_query(ent->query);
			}

			// draw depth buffer
			gfx_set_shader_params_default(&scene->shader_params);
			elf_set_camera(scene->cur_camera, &scene->shader_params);
			scene->shader_params.render_params.color_write = ELF_FALSE;
			scene->shader_params.render_params.alpha_write = ELF_FALSE;
			scene->shader_params.gbuffer = eng->gbuffer;
			scene->shader_params.gbuffer_mode = GFX_GBUFFER_DEPTH;

			for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
				i < scene->entity_queue_count && ent != NULL;
				i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
			{
				if(ent->occluder) continue;

				if(gfx_get_query_result(ent->query) < 1)
				{
					elf_remove_from_list(scene->entity_queue, (elf_object*)ent);
					scene->entity_queue_count--; i--;
				}
				else
				{
					elf_draw_entity(ent, ELF_DRAW_DEPTH, &scene->shader_params);
				}
			}

			for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
				i < scene->sprite_queue_count && spr != NULL;
				i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
			{
				if(spr->occluder) continue;

				elf_draw_sprite(spr, ELF_DRAW_DEPTH, &scene->shader_params);
			}
		}
		else
		{
			// draw depth buffer
			gfx_set_shader_params_default(&scene->shader_params);
			elf_set_camera(scene->cur_camera, &scene->shader_params);
			scene->shader_params.render_params.color_write = ELF_FALSE;
			scene->shader_params.render_params.alpha_write = ELF_FALSE;
			scene->shader_params.gbuffer = eng->gbuffer;
			scene->shader_params.gbuffer_mode = GFX_GBUFFER_DEPTH;

			for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
				i < scene->entity_queue_count && ent != NULL;
				i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
			{
				elf_draw_entity(ent, ELF_DRAW_DEPTH, &scene->shader_params);
			}

			for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
				i < scene->sprite_queue_count && spr != NULL;
				i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
			{
				elf_draw_sprite(spr, ELF_DRAW_DEPTH, &scene->shader_params);
			}
		}
	}
	else
	{
		// draw depth buffer
		gfx_set_shader_params_default(&scene->shader_params);
		elf_set_camera(scene->cur_camera, &scene->shader_params);
		scene->shader_params.render_params.color_write = ELF_FALSE;
		scene->shader_params.render_params.alpha_write = ELF_FALSE;
		scene->shader_params.gbuffer = eng->gbuffer;
		scene->shader_params.gbuffer_mode = GFX_GBUFFER_DEPTH;

		scene->entity_queue_count = 0;
		elf_begin_list(scene->entity_queue);

		for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
			ent = (elf_entity*)elf_next_in_list(scene->entities))
		{
			if(!elf_cull_entity(ent, scene->cur_camera))
			{
				if(scene->entity_queue_count < elf_get_list_length(scene->entity_queue))
				{
					elf_set_list_cur_ptr(scene->entity_queue, (elf_object*)ent);
					elf_next_in_list(scene->entity_queue);
				}
				else
				{
					elf_append_to_list(scene->entity_queue, (elf_object*)ent);
				}
				scene->entity_queue_count++;
				elf_draw_entity(ent, ELF_DRAW_DEPTH, &scene->shader_params);
				ent->culled = ELF_FALSE;
			}
			else
			{
				ent->culled = ELF_TRUE;
			}
		}

		scene->sprite_queue_count = 0;
		elf_begin_list(scene->sprite_queue);

		for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
			spr = (elf_sprite*)elf_next_in_list(scene->sprites))
		{
			if(!elf_cull_sprite(spr, scene->cur_camera))
			{
				if(scene->sprite_queue_count < elf_get_list_length(scene->sprite_queue))
				{
					elf_set_list_cur_ptr(scene->sprite_queue, (elf_object*)spr);
					elf_next_in_list(scene->sprite_queue);
				}
				else
				{
					elf_append_to_list(scene->sprite_queue, (elf_object*)spr);
				}
				scene->sprite_queue_count++;
				elf_draw_sprite(spr, ELF_DRAW_DEPTH, &scene->shader_params);
				spr->culled = ELF_FALSE;
			}
			else
			{
				spr->culled = ELF_TRUE;
			}
		}
	}

	// draw gbuffers
	gfx_set_shader_params_default(&scene->shader_params);
	elf_set_camera(scene->cur_camera, &scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_func = GFX_EQUAL;
	scene->shader_params.gbuffer = eng->gbuffer;
	scene->shader_params.gbuffer_mode = GFX_GBUFFER_FILL;

	for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
		i < scene->entity_queue_count && ent != NULL;
		i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
	{
		elf_draw_entity(ent, ELF_DRAW_WITH_LIGHTING, &scene->shader_params);
	}

	for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
		i < scene->sprite_queue_count && spr != NULL;
		i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
	{
		elf_draw_sprite(spr, ELF_DRAW_WITH_LIGHTING, &scene->shader_params);
	}

	// get inverse projection matrix for retrieving vertex position from depth
	gfx_matrix4_get_inverse(scene->cur_camera->projection_matrix, inv_projection_matrix);

	// draw lighting
	gfx_bind_gbuffer_light(eng->gbuffer, &scene->shader_params);
	gfx_clear_color_buffer(0.0, 0.0, 0.0, 1.0);

	cam_orient = elf_get_actor_orientation((elf_actor*)scene->cur_camera);
	wwidth = elf_get_window_width();
	wheight = elf_get_window_height();

	for(lig = (elf_light*)elf_begin_list(scene->lights); lig;
		lig = (elf_light*)elf_next_in_list(scene->lights))
	{
		if(!lig->visible) continue;

		// cull lights that aren't affecting any pixels on the screen
		lpos = elf_get_actor_position((elf_actor*)lig);
		ldist = lig->distance+1.0/lig->fade_speed;

		ldvec.x = 0.0; ldvec.y = ldist; ldvec.z = 0.0;
		ldvec = elf_mul_qua_vec3f(cam_orient, ldvec);
		ldvec = elf_add_vec3f_vec3f(lpos, ldvec);

		lsspos = elf_project_camera_point(scene->cur_camera, lpos.x, lpos.y, lpos.z);
		ldsspos = elf_project_camera_point(scene->cur_camera, ldvec.x, ldvec.y, ldvec.z);

		ldvec = elf_sub_vec3f_vec3f(ldsspos, lsspos);
		ldist = elf_get_vec3f_length(ldvec);

		lmin.x = lsspos.x-ldist;
		lmin.y = lsspos.y-ldist;
		lmax.x = lsspos.x+ldist;
		lmax.y = lsspos.y+ldist;

		if(lmin.x < 0) lmin.x = 0;
		if(lmin.y < 0) lmin.y = 0;
		if(lmin.x > wwidth) lmin.x = wwidth;
		if(lmin.y > wheight) lmin.y = wheight;
		if(lmax.x < 0) lmax.x = 0;
		if(lmax.y < 0) lmax.y = 0;
		if(lmax.x > wwidth) lmax.x = wwidth;
		if(lmax.y > wheight) lmax.y = wheight;

		if(lmax.x-lmin.x == 0) continue;
		if(lmax.y-lmin.y == 0) continue;

		// need to call this for frustum culling...
		if(lig->light_type == ELF_SPOT_LIGHT)
		{
			elf_set_camera_viewport(lig->shadow_camera, 0, 0, elf_get_shadow_map_size(), elf_get_shadow_map_size());
			elf_set_camera(lig->shadow_camera, &scene->shader_params);

			// check are there any entities visible for the spot, if there aren't don't bother continuing, just skip to the next light
			found = ELF_FALSE;
			for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
				i < scene->entity_queue_count && ent != NULL;
				i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
			{
				if(!elf_cull_entity(ent, lig->shadow_camera))
				{
					found = ELF_TRUE;
					break;
				}
			}

			for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
				i < scene->sprite_queue_count && spr != NULL;
				i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
			{
				if(!elf_cull_sprite(spr, lig->shadow_camera))
				{
					found = ELF_TRUE;
					break;
				}
			}

			if(!found) continue;

			// render shadow map if needed
			if(lig->shadows)
			{
				gfx_disable_scissor();

				gfx_set_shader_params_default(&scene->shader_params);
				scene->shader_params.render_params.color_write = GFX_FALSE;
				scene->shader_params.render_params.alpha_write = GFX_FALSE;
				scene->shader_params.render_params.offset_bias = 2.0;
				scene->shader_params.render_params.offset_scale = 4.0;
				elf_set_camera(lig->shadow_camera, &scene->shader_params);
				gfx_set_shader_params(&scene->shader_params);

				gfx_set_render_target(eng->shadow_target);
				gfx_clear_depth_buffer(1.0);

				for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
					ent = (elf_entity*)elf_next_in_list(scene->entities))
				{
					if(!elf_cull_entity(ent, lig->shadow_camera))
					{
						elf_draw_entity(ent, ELF_DRAW_DEPTH, &scene->shader_params);
					}
				}

				for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
					spr = (elf_sprite*)elf_next_in_list(scene->sprites))
				{
					if(!elf_cull_sprite(spr, lig->shadow_camera))
					{
						elf_draw_sprite(spr, ELF_DRAW_DEPTH, &scene->shader_params);
					}
				}

				gfx_mul_matrix4_matrix4(elf_get_camera_projection_matrix(lig->shadow_camera), bias, temp_mat1);
				gfx_mul_matrix4_matrix4(elf_get_camera_modelview_matrix(lig->shadow_camera), temp_mat1, temp_mat2);
				gfx_matrix4_get_inverse_fast(elf_get_camera_modelview_matrix(scene->cur_camera), temp_mat1);
				gfx_mul_matrix4_matrix4(temp_mat1, temp_mat2, lig->projection_matrix);
			}
		}

		gfx_set_scissor((int)lmin.x, (int)lmin.y, (int)(lmax.x-lmin.x), (int)(lmax.y-lmin.y));

		gfx_bind_gbuffer_light(eng->gbuffer, &scene->shader_params);

		gfx_set_shader_params_default(&scene->shader_params);

		if(lig->light_type == ELF_SPOT_LIGHT && lig->shadows)
		{
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].type = GFX_SHADOW_MAP;
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].texture = eng->shadow_map;
			scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].projection_mode = GFX_SHADOW_PROJECTION;
			memcpy(scene->shader_params.texture_params[GFX_MAX_TEXTURES-1].matrix,
				lig->projection_matrix, sizeof(float)*16);
		}

		gfx_set_viewport(0, 0, elf_get_window_width(), elf_get_window_height());
		gfx_get_orthographic_projection_matrix(0.0, (float)elf_get_window_width(),
			0.0, (float)elf_get_window_height(), -1.0, 1.0,
			scene->shader_params.projection_matrix);
		scene->shader_params.render_params.depth_test = GFX_FALSE;
		scene->shader_params.render_params.depth_write = GFX_FALSE;
		scene->shader_params.render_params.blend_mode = GFX_ADD;
		scene->shader_params.gbuffer_mode = GFX_GBUFFER_LIGHTING;
		memcpy(scene->shader_params.inv_projection_matrix, inv_projection_matrix, sizeof(float)*16);
		scene->shader_params.texture_params[0].texture = gfx_get_gbuffer_depth(eng->gbuffer);
		scene->shader_params.texture_params[1].texture = gfx_get_gbuffer_buf1(eng->gbuffer);
		scene->shader_params.texture_params[2].texture = gfx_get_gbuffer_buf3(eng->gbuffer);

		elf_set_light(lig, scene->cur_camera, &scene->shader_params);

		gfx_set_shader_params(&scene->shader_params);

		gfx_draw_textured_2d_quad(0.0, 0.0, (float)elf_get_window_width(), (float)elf_get_window_height());
	}

	gfx_disable_scissor();

	gfx_bind_gbuffer_main(eng->gbuffer, &scene->shader_params);

	// combine lighting
	gfx_set_shader_params_default(&scene->shader_params);
	scene->shader_params.render_params.depth_test = GFX_FALSE;
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.shader_program = scene->compose_main_shdr;
	scene->shader_params.texture_params[0].texture = gfx_get_gbuffer_buf2(eng->gbuffer);
	scene->shader_params.texture_params[1].texture = gfx_get_gbuffer_diffuse(eng->gbuffer);
	scene->shader_params.texture_params[2].texture = gfx_get_gbuffer_specular(eng->gbuffer);

	gfx_set_viewport(0, 0, elf_get_window_width(), elf_get_window_height());
	gfx_get_orthographic_projection_matrix(0.0, (float)elf_get_window_width(),
		0.0, (float)elf_get_window_height(), -1.0, 1.0,
		scene->shader_params.projection_matrix);
	
	gfx_set_shader_params(&scene->shader_params);

	gfx_draw_textured_2d_quad(0.0, 0.0, (float)elf_get_window_width(), (float)elf_get_window_height());

	// draw ambient
	if(!elf_about_zero(scene->ambient_color.r) ||
		!elf_about_zero(scene->ambient_color.g) ||
		!elf_about_zero(scene->ambient_color.b) )
	{
		scene->shader_params.shader_program = NULL;
		scene->shader_params.render_params.blend_mode = GFX_ADD;
		scene->shader_params.texture_params[0].texture = gfx_get_gbuffer_buf2(eng->gbuffer);
		scene->shader_params.texture_params[1].texture = NULL;
		scene->shader_params.texture_params[2].texture = NULL;
		memcpy(&scene->shader_params.material_params.color.r, &scene->ambient_color.r, sizeof(float)*4);

		gfx_set_shader_params(&scene->shader_params);

		gfx_draw_textured_2d_quad(0.0, 0.0, (float)elf_get_window_width(), (float)elf_get_window_height());
	}

	// draw non lighted stuff
	gfx_set_shader_params_default(&scene->shader_params);
	elf_set_camera(scene->cur_camera, &scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_func = GFX_EQUAL;

	for(i = 0, ent = (elf_entity*)elf_begin_list(scene->entity_queue);
		i < scene->entity_queue_count && ent != NULL;
		i++, ent = (elf_entity*)elf_next_in_list(scene->entity_queue))
	{
		elf_draw_entity(ent, ELF_DRAW_WITHOUT_LIGHTING, &scene->shader_params);
	}

	for(i = 0, spr = (elf_sprite*)elf_begin_list(scene->sprite_queue);
		i < scene->sprite_queue_count && spr != NULL;
		i++, spr = (elf_sprite*)elf_next_in_list(scene->sprite_queue))
	{
		elf_draw_sprite(spr, ELF_DRAW_WITHOUT_LIGHTING, &scene->shader_params);
	}

	// render particles
	gfx_set_shader_params_default(&scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_func = GFX_LEQUAL;
	scene->shader_params.render_params.color_write = GFX_TRUE;
	scene->shader_params.render_params.alpha_write = GFX_TRUE;
	elf_set_camera(scene->cur_camera, &scene->shader_params);
	
	for(par = (elf_particles*)elf_begin_list(scene->particles); par;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		if(!elf_cull_particles(par, scene->cur_camera))
		{
			elf_draw_particles(par, scene->cur_camera, &scene->shader_params);
		}
	}

	gfx_disable_render_target();

	if(!eng->post_process)
	{
		// draw final image
		gfx_set_shader_params_default(&scene->shader_params);
		scene->shader_params.render_params.depth_test = GFX_FALSE;
		scene->shader_params.render_params.depth_write = GFX_FALSE;
		scene->shader_params.texture_params[0].texture = gfx_get_gbuffer_main(eng->gbuffer);

		gfx_set_viewport(0, 0, elf_get_window_width(), elf_get_window_height());
		gfx_get_orthographic_projection_matrix(0.0, (float)elf_get_window_width(),
			0.0, (float)elf_get_window_height(), -1.0, 1.0,
			scene->shader_params.projection_matrix);
	
		gfx_set_shader_params(&scene->shader_params);

		gfx_draw_textured_2d_quad(0.0, 0.0, (float)elf_get_window_width(), (float)elf_get_window_height());
	}

	// reset state just to be sure...
	gfx_set_shader_params_default(&scene->shader_params);
	gfx_set_shader_params(&scene->shader_params);

	// keep the query lists compact

	if(elf_get_list_length(scene->entity_queue) > scene->entity_queue_count)
	{
		ent = (elf_entity*)elf_rbegin_list(scene->entity_queue);
		if(ent) elf_remove_from_list(scene->entity_queue, (elf_object*)ent);
	}

	if(elf_get_list_length(scene->sprite_queue) > scene->sprite_queue_count)
	{
		spr = (elf_sprite*)elf_rbegin_list(scene->sprite_queue);
		if(spr) elf_remove_from_list(scene->sprite_queue, (elf_object*)spr);
	}
}*/

void elf_draw_scene_debug(elf_scene *scene)
{
	elf_entity *ent;
	elf_light *lig;
	elf_camera *cam;
	elf_particles *par;
	elf_sprite *spr;

	if(!scene->cur_camera) return;

	gfx_set_shader_params_default(&scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_test = GFX_FALSE;
	scene->shader_params.render_params.blend_mode = GFX_ADD;
	elf_set_camera(scene->cur_camera, &scene->shader_params);

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent != NULL;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		elf_draw_entity_debug(ent, &scene->shader_params);
	}

	gfx_set_shader_params_default(&scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_test = GFX_FALSE;
	scene->shader_params.render_params.blend_mode = GFX_ADD;
	elf_set_camera(scene->cur_camera, &scene->shader_params);

	for(par = (elf_particles*)elf_begin_list(scene->particles); par != NULL;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		elf_draw_particles_debug(par, &scene->shader_params);
	}

	gfx_set_shader_params_default(&scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_test = GFX_FALSE;
	scene->shader_params.render_params.blend_mode = GFX_ADD;
	elf_set_camera(scene->cur_camera, &scene->shader_params);

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr != NULL;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		elf_draw_sprite_debug(spr, &scene->shader_params);
	}

	gfx_set_shader_params_default(&scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_test = GFX_FALSE;
	scene->shader_params.render_params.blend_mode = GFX_ADD;
	elf_set_camera(scene->cur_camera, &scene->shader_params);

	for(lig = (elf_light*)elf_begin_list(scene->lights); lig != NULL;
		lig = (elf_light*)elf_next_in_list(scene->lights))
	{
		elf_draw_light_debug(lig, &scene->shader_params);
	}

	gfx_set_shader_params_default(&scene->shader_params);
	scene->shader_params.render_params.depth_write = GFX_FALSE;
	scene->shader_params.render_params.depth_test = GFX_FALSE;
	scene->shader_params.render_params.blend_mode = GFX_ADD;
	elf_set_camera(scene->cur_camera, &scene->shader_params);

	for(cam = (elf_camera*)elf_begin_list(scene->cameras); cam != NULL;
		cam = (elf_camera*)elf_next_in_list(scene->cameras))
	{
		if(cam == scene->cur_camera) continue;
		elf_draw_camera_debug(cam, &scene->shader_params);
	}

	// reset state just to be sure...
	gfx_set_shader_params_default(&scene->shader_params);
	gfx_set_shader_params(&scene->shader_params);
}

elf_list* elf_get_scene_scripts(elf_scene *scene)
{
	elf_list *scripts;

	elf_camera *cam;
	elf_entity *ent;
	elf_light *lig;
	elf_particles *par;
	elf_sprite *spr;

	scripts = elf_create_list();

	for(cam = (elf_camera*)elf_begin_list(scene->cameras); cam;
		cam = (elf_camera*)elf_next_in_list(scene->cameras))
	{
		if(cam->script && !elf_get_resource_by_id(scripts, cam->script->id))
		{
			elf_append_to_list(scripts, (elf_object*)cam->script);
		}
	}

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		if(ent->script && !elf_get_resource_by_id(scripts, ent->script->id))
		{
			elf_append_to_list(scripts, (elf_object*)ent->script);
		}
	}

	for(lig = (elf_light*)elf_begin_list(scene->lights); lig;
		lig = (elf_light*)elf_next_in_list(scene->lights))
	{
		if(lig->script && !elf_get_resource_by_id(scripts, lig->script->id))
		{
			elf_append_to_list(scripts, (elf_object*)lig->script);
		}
	}

	for(par = (elf_particles*)elf_begin_list(scene->particles); par;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		if(par->script && !elf_get_resource_by_id(scripts, par->script->id))
		{
			elf_append_to_list(scripts, (elf_object*)par->script);
		}
	}

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		if(spr->script && !elf_get_resource_by_id(scripts, spr->script->id))
		{
			elf_append_to_list(scripts, (elf_object*)spr->script);
		}
	}

	return scripts;
}

elf_list* elf_get_scene_textures(elf_scene *scene)
{
	elf_list *textures;

	elf_material *mat;
	elf_entity *ent;
	elf_particles *par;
	elf_sprite *spr;

	textures = elf_create_list();

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		for(mat = (elf_material*)elf_begin_list(ent->materials); mat;
			mat = (elf_material*)elf_next_in_list(ent->materials))
		{
			if(mat->diffuse_map && !elf_get_resource_by_id(textures, mat->diffuse_map->id))
				elf_append_to_list(textures, (elf_object*)mat->diffuse_map);
			if(mat->normal_map && !elf_get_resource_by_id(textures, mat->normal_map->id))
				elf_append_to_list(textures, (elf_object*)mat->normal_map);
			if(mat->height_map && !elf_get_resource_by_id(textures, mat->height_map->id))
				elf_append_to_list(textures, (elf_object*)mat->height_map);
			if(mat->specular_map && !elf_get_resource_by_id(textures, mat->specular_map->id))
				elf_append_to_list(textures, (elf_object*)mat->specular_map);
			if(mat->light_map && !elf_get_resource_by_id(textures, mat->light_map->id))
				elf_append_to_list(textures, (elf_object*)mat->light_map);
		}
	}

	for(par = (elf_particles*)elf_begin_list(scene->particles); par;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		if(par->texture && !elf_get_resource_by_id(textures, par->texture->id))
			elf_append_to_list(textures, (elf_object*)par->texture);
	}

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		mat = spr->material;
		if(!mat) continue;

		if(mat->diffuse_map && !elf_get_resource_by_id(textures, mat->diffuse_map->id))
			elf_append_to_list(textures, (elf_object*)mat->diffuse_map);
		if(mat->normal_map && !elf_get_resource_by_id(textures, mat->normal_map->id))
			elf_append_to_list(textures, (elf_object*)mat->normal_map);
		if(mat->height_map && !elf_get_resource_by_id(textures, mat->height_map->id))
			elf_append_to_list(textures, (elf_object*)mat->height_map);
		if(mat->specular_map && !elf_get_resource_by_id(textures, mat->specular_map->id))
			elf_append_to_list(textures, (elf_object*)mat->specular_map);
		if(mat->light_map && !elf_get_resource_by_id(textures, mat->light_map->id))
			elf_append_to_list(textures, (elf_object*)mat->light_map);
	}

	return textures;
}

elf_list* elf_get_scene_materials(elf_scene *scene)
{
	elf_list *materials;

	elf_material *mat;
	elf_entity *ent;
	elf_sprite *spr;

	materials = elf_create_list();

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		for(mat = (elf_material*)elf_begin_list(ent->materials); mat;
			mat = (elf_material*)elf_next_in_list(ent->materials))
		{
			if(!elf_get_resource_by_id(materials, mat->id))
			{
				elf_append_to_list(materials, (elf_object*)mat);
			}
		}
	}

	for(spr = (elf_sprite*)elf_begin_list(scene->sprites); spr;
		spr = (elf_sprite*)elf_next_in_list(scene->sprites))
	{
		if(spr->material && !elf_get_resource_by_id(materials, spr->material->id))
		{
			elf_append_to_list(materials, (elf_object*)spr->material);
		}
	}

	return materials;
}



elf_list* elf_get_scene_models(elf_scene *scene)
{
	elf_list *models;

	elf_entity *ent;
	elf_particles *par;

	models = elf_create_list();

	for(ent = (elf_entity*)elf_begin_list(scene->entities); ent;
		ent = (elf_entity*)elf_next_in_list(scene->entities))
	{
		if(ent->model && !elf_get_resource_by_id(models, ent->model->id))
		{
			elf_append_to_list(models, (elf_object*)ent->model);
		}
	}

	for(par = (elf_particles*)elf_begin_list(scene->particles); par;
		par = (elf_particles*)elf_next_in_list(scene->particles))
	{
		if(par->model && !elf_get_resource_by_id(models, par->model->id))
		{
			elf_append_to_list(models, (elf_object*)par->model);
		}
	}

	return models;
}

