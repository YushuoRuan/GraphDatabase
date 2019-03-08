#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "config.h"
#include "graph.h"
#include "cli.h"
#include "tuple.h"

/* Place the code for your Dijkstra implementation in this file */

struct vertexNode
{
	vertexid_t id;
	int visited;
	int dist;
	struct vertexNode* previous;
};

vertexid_t* loadVertices(int countV);
int count_vertices();
int weight(vertexid_t i, vertexid_t j, component_t c);
struct vertexNode* minD(struct vertexNode *vertices, int numVertices);

int component_sssp(
        component_t c,
        vertexid_t v1,
        vertexid_t v2,
        int *n,
        int *total_weight,
        vertexid_t **path)
{
	attribute_t current_attribute;
	int found = 0;
	current_attribute = c->se->attrlist;
	while (found==0 && current_attribute != NULL)
	{
		if (current_attribute->bt == INTEGER) {
			found = 1;
		}
		else {
			current_attribute = current_attribute->next;
		}
	}
	if (found == 0)
	{
		//printf("Error! Weight not found.\n");
		return -1;
	}

	int numVertices = count_vertices();
	vertexid_t* vertList = loadVertices(numVertices);
	struct vertexNode *vertices= malloc(sizeof(struct vertexNode)* numVertices);

	int source = 0;
	int end = 0;
	for (int i = 0; i < numVertices; i++)
	{
		struct vertexNode vert;
		vert.id = vertList[i];
		vert.dist = weight(v1, vertList[i], c);
		if (vert.id == v1) {
			vert.visited = 1;
			source = i;
		}
		else
			vert.visited = 0;
		if (vert.id == v2)
			end = i;
		vertices[i] = vert;
	}

	for (int i = 0; i < numVertices; i++){
		if (vertices[i].id != v1 && vertices[i].dist < INT_MAX)
			vertices[i].previous = &vertices[source];
	}

#if _DEBUG
	for (int i = 0; i < numVertices; i++)
	{
		if(vertices[i].previous)
			printf("vertex--%llu, distance--%d, visited--%d, previous--%llu\n", vertices[i].id, vertices[i].dist, vertices[i].visited, vertices[i].previous->id);
		else
			printf("vertex--%llu, distance--%d, visited--%d, no previous\n", vertices[i].id, vertices[i].dist, vertices[i].visited);
	}
#endif

	for (int i = 0; i < numVertices-1; i++)
	{
		struct vertexNode *minVert = minD(vertices, numVertices);

		minVert->visited = 1;
		for (int j = 0; j < numVertices; j++)
		{
			if (vertices[j].visited == 1) {
				continue;
			}
			if (weight(minVert->id, vertices[j].id, c) == INT_MAX) {
				continue;
			}
			int newDist = minVert->dist + weight(minVert->id, vertices[j].id, c);
			if (newDist < vertices[j].dist) {
				vertices[j].dist = newDist;
				vertices[j].previous = minVert;
			}
		}
	}

	*total_weight = vertices[end].dist;

	struct vertexNode* ptr = &vertices[end];
	int count = 0;
	while (ptr)
	{
		count++;
		ptr = ptr->previous;
	}
	*n = count;

	ptr = &vertices[end];
	*path = malloc(sizeof(vertexid_t)* *n);
	for (int i = *n; i > 0; i--)
	{
		path[i - 1] = &ptr->id;
		ptr = ptr->previous;
	}
	
	printf("shortest distance from %llu to %llu is %d\n", v1, v2, *total_weight);
	printf("passed %d vertices: ", *n);
	for (int i = 0; i < *n; i++)
	{
		printf("%llu", *path[i]);
		if (i != *n - 1)
			printf(", ");
	}
	printf("\n");
	
	return 0;
}

vertexid_t* loadVertices(int countV) {

	vertexid_t* vertices;
	off_t off;
	char s[BUFSIZE];
	int fd = -1;
	char *buf;
	vertex_t v;
	vertexid_t id;
	ssize_t len;
	long unsigned int vertex_size = sizeof(v);
	memset(s, 0, BUFSIZE);
	sprintf(s, "%s/%d/%d/%s", grdbdir, gno, cno, "v");
	fd = open(s, O_RDONLY);

	buf = malloc(vertex_size);
	vertices = malloc(sizeof(vertexid_t) * countV);

	int i = 0;
	for (off = 0;; off += vertex_size) {
		lseek(fd, off, SEEK_SET);
		len = read(fd, buf, vertex_size);
		if (len <= 0)
			break;
		id = *((vertexid_t *)buf);
		vertices[i] = id;
		i++;
	}
	return vertices;
}

int count_vertices() {
	char s[BUFSIZE];
	int fd, sz = -1;
	long unsigned int vertex_size;
	struct stat buf;
	vertex_t v;
	memset(s, 0, BUFSIZE);
	sprintf(s, "%s/%d/%d/%s", grdbdir, gno, cno, "v");
	fd = open(s, O_RDONLY);
	fstat(fd, &buf);
	sz = buf.st_size;
	close(fd);
	vertex_size = sizeof(v);
	sz = sz / vertex_size;
	return sz;
}

int weight(vertexid_t i, vertexid_t j, component_t c) {
	schema_t se = c->se;
	char* name = c->se->attrlist->name;
	char s[BUFSIZE];
	int fd = -1;
	int offset = -1;
	int weight;
	memset(s, 0, BUFSIZE);
	sprintf(s, "%s/%d/%d/%s", grdbdir, gno, cno, "e");
	fd = open(s, O_RDONLY);
	edge_t e = malloc(sizeof(struct edge));
	edge_init(e);
	e->id1 = i;
	e->id2 = j;
	edge_read(e, se, fd);
	if (i == j)
		return 0;
	if (e->tuple)
	{
		offset = tuple_get_offset(e->tuple, name);

		weight = tuple_get_int(e->tuple->buf + offset);
		close(fd);
		free(e);
		return weight;
	}
	return INT_MAX;

}

struct vertexNode* minD(struct vertexNode *vertices, int numVertices)
{
	int min = INT_MAX;
	int found = 0;
	int index = 0;
	for (int i = 0; i < numVertices && !found; i++)
	{
		if (vertices[i].visited)
			continue;
		if (vertices[i].dist <= min) {
			min = vertices[i].dist;
			index = i;
			found = 1;
		}
	}
	if (found == 0)
		return NULL;

	return &vertices[index];
}
