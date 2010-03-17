/*
 * Copyright 2009 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      Lukas Kuklinek <lkuklinek@redhat.com>
 */

#include "item.h"
#include "helpers.h"
#include <string.h>
#include <math.h>

struct xccdf_item *xccdf_item_new(xccdf_type_t type, struct xccdf_item *parent)
{
	struct xccdf_item *item;
	size_t size = sizeof(*item) - sizeof(item->sub);

	switch (type) {
	case XCCDF_BENCHMARK:
		size += sizeof(struct xccdf_benchmark_item);
		break;
	case XCCDF_RULE:
		size += sizeof(struct xccdf_rule_item);
		break;
	case XCCDF_GROUP:
		size += sizeof(struct xccdf_group_item);
		break;
	case XCCDF_VALUE:
		size += sizeof(struct xccdf_value_item);
		break;
	case XCCDF_RESULT:
		size += sizeof(struct xccdf_result_item);
		break;
	default:
		size += sizeof(item->sub);
		break;
	}

	item = oscap_calloc(1, size);
	item->type = type;
	item->item.title = oscap_list_new();
	item->item.description = oscap_list_new();
	item->item.question = oscap_list_new();
	item->item.rationale = oscap_list_new();
	item->item.statuses = oscap_list_new();
	item->item.platforms = oscap_list_new();
	item->item.warnings = oscap_list_new();
	item->item.references = oscap_list_new();
	item->item.weight = 1.0;
	item->item.flags.selected = true;
	item->item.parent = parent;
	return item;
}

void xccdf_item_release(struct xccdf_item *item)
{
	if (item) {
		oscap_list_free(item->item.statuses, (oscap_destruct_func) xccdf_status_free);
		oscap_list_free(item->item.platforms, oscap_free);
		oscap_list_free(item->item.title, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(item->item.description, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(item->item.rationale, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(item->item.question, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(item->item.warnings, (oscap_destruct_func) xccdf_warning_free);
		oscap_list_free(item->item.references, (oscap_destruct_func) xccdf_reference_free);
		oscap_free(item->item.id);
		oscap_free(item->item.cluster_id);
		oscap_free(item->item.version_update);
		oscap_free(item->item.version);
		oscap_free(item->item.extends);
		oscap_free(item);
	}
}

void xccdf_item_free(struct xccdf_item *item)
{
	if (item == NULL)
		return;
	switch (item->type) {
	case XCCDF_BENCHMARK:
		xccdf_benchmark_free(XBENCHMARK(item));
		break;
	case XCCDF_GROUP:
		xccdf_group_free(item);
		break;
	case XCCDF_RULE:
		xccdf_rule_free(item);
		break;
	case XCCDF_VALUE:
		xccdf_value_free(item);
		break;
	case XCCDF_RESULT:
		xccdf_result_free(XRESULT(item));
		break;
	default:
		assert((fprintf(stderr, "Deletion of item of type no. %u is not yet supported.", item->type), false));
	}
}

void xccdf_item_dump(struct xccdf_item *item, int depth)
{
	if (item == NULL)
		return;
	switch (item->type) {
	case XCCDF_BENCHMARK:
		xccdf_benchmark_dump(XBENCHMARK(item));
		break;
	case XCCDF_GROUP:
		xccdf_group_dump(item, depth);
		break;
	case XCCDF_RULE:
		xccdf_rule_dump(item, depth);
		break;
	default:
		xccdf_print_depth(depth);
		fprintf(stderr, "I cannot yet dump an item of type no. %u.", item->type);
	}
}

void xccdf_item_print(struct xccdf_item *item, int depth)
{
	if (item) {
		if (item->item.parent) {
			xccdf_print_depth(depth);
			printf("parent  : %s\n", item->item.parent->item.id);
		}
		if (item->item.extends) {
			xccdf_print_depth(depth);
			printf("extends : %s\n", item->item.extends);
		}
		if (item->type == XCCDF_BENCHMARK) {
			xccdf_print_depth(depth);
			printf("resolved: %d\n", item->item.flags.resolved);
		}
		if (item->type & XCCDF_CONTENT) {
			xccdf_print_depth(depth);
			printf("selected: %d\n", item->item.flags.selected);
		}
		if (item->item.version) {
			xccdf_print_depth(depth);
			printf("version : %s\n", item->item.version);
		}
		xccdf_print_depth(depth);
		printf("title   : ");
        xccdf_print_textlist(oscap_iterator_new(item->item.title), depth + 1, 70, "...");
		xccdf_print_depth(depth);
		printf("desc    : ");
        xccdf_print_textlist(oscap_iterator_new(item->item.description), depth + 1, 70, "...");
		xccdf_print_depth(depth);
		printf("platforms ");
		oscap_list_dump(item->item.platforms, xccdf_cstring_dump, depth + 1);
		xccdf_print_depth(depth);
		printf("status (cur = %d)", xccdf_item_get_current_status(item));
		oscap_list_dump(item->item.statuses, xccdf_status_dump, depth + 1);
	}
}

#define XCCDF_ITEM_PROCESS_FLAG(reader,flag,attr) \
	if (xccdf_attribute_has((reader), (attr))) \
		item->item.flags.flag = xccdf_attribute_get_bool((reader), (attr));

bool xccdf_item_process_attributes(struct xccdf_item *item, xmlTextReaderPtr reader)
{
	item->item.id = xccdf_attribute_copy(reader, XCCDFA_ID);

	XCCDF_ITEM_PROCESS_FLAG(reader, resolved, XCCDFA_RESOLVED);
	XCCDF_ITEM_PROCESS_FLAG(reader, hidden, XCCDFA_HIDDEN);
	XCCDF_ITEM_PROCESS_FLAG(reader, selected, XCCDFA_SELECTED);
	XCCDF_ITEM_PROCESS_FLAG(reader, prohibit_changes, XCCDFA_PROHIBITCHANGES);
	XCCDF_ITEM_PROCESS_FLAG(reader, multiple, XCCDFA_MULTIPLE);
	XCCDF_ITEM_PROCESS_FLAG(reader, abstract, XCCDFA_ABSTRACT);
	XCCDF_ITEM_PROCESS_FLAG(reader, interactive, XCCDFA_INTERACTIVE);

	if (xccdf_attribute_has(reader, XCCDFA_WEIGHT))
		item->item.weight = xccdf_attribute_get_float(reader, XCCDFA_WEIGHT);
	if (xccdf_attribute_has(reader, XCCDFA_EXTENDS))
		item->item.extends = xccdf_attribute_copy(reader, XCCDFA_EXTENDS);
	item->item.cluster_id = xccdf_attribute_copy(reader, XCCDFA_CLUSTER_ID);

	if (item->item.id) {
		struct xccdf_item *bench = XITEM(xccdf_item_get_benchmark_internal(item));
		if (bench != NULL)
			oscap_htable_add(bench->sub.bench.dict, item->item.id, item);
	}
	return item->item.id != NULL;
}

bool xccdf_item_process_element(struct xccdf_item * item, xmlTextReaderPtr reader)
{
	xccdf_element_t el = xccdf_element_get(reader);

	switch (el) {
	case XCCDFE_TITLE:
        oscap_list_add(item->item.title, oscap_text_new_parse(XCCDF_TEXT_PLAIN, reader));
		return true;
	case XCCDFE_DESCRIPTION:
        oscap_list_add(item->item.description, oscap_text_new_parse(XCCDF_TEXT_HTMLSUB, reader));
		return true;
	case XCCDFE_WARNING:
        oscap_list_add(item->item.warnings, xccdf_warning_new_parse(reader));
		return true;
	case XCCDFE_REFERENCE:
        oscap_list_add(item->item.references, xccdf_reference_new_parse(reader));
		return true;
	case XCCDFE_STATUS:{
        const char *date = xccdf_attribute_get(reader, XCCDFA_DATE);
        char *str = oscap_element_string_copy(reader);
        struct xccdf_status *status = xccdf_status_new(str, date);
        oscap_free(str);
        if (status) {
            oscap_list_add(item->item.statuses, status);
            return true;
        }
        break;
    }
	case XCCDFE_VERSION:
		item->item.version_time = oscap_get_datetime(xccdf_attribute_get(reader, XCCDFA_TIME));
		item->item.version_update = xccdf_attribute_copy(reader, XCCDFA_UPDATE);
		item->item.version = oscap_element_string_copy(reader);
		break;
	case XCCDFE_RATIONALE:
        oscap_list_add(item->item.rationale, oscap_text_new_parse(XCCDF_TEXT_HTMLSUB, reader));
		return true;
	case XCCDFE_QUESTION:
        oscap_list_add(item->item.question, oscap_text_new_parse(XCCDF_TEXT_PLAIN, reader));
		return true;
	default:
		break;
	}
	return false;
}

inline struct xccdf_item* xccdf_item_get_benchmark_internal(struct xccdf_item* item)
{
	if (item == NULL) return NULL;
	while (xccdf_item_get_parent(item) != NULL)
		item = xccdf_item_get_parent(item);
	return (xccdf_item_get_type(item) == XCCDF_BENCHMARK ? item : NULL);
}

#define XCCDF_BENCHGETTER(TYPE) \
	struct xccdf_benchmark* xccdf_##TYPE##_get_benchmark(const struct xccdf_##TYPE* item) \
	{ return XBENCHMARK(xccdf_item_get_benchmark_internal(XITEM(item))); }
XCCDF_BENCHGETTER(item)  XCCDF_BENCHGETTER(profile) XCCDF_BENCHGETTER(rule)
XCCDF_BENCHGETTER(group) XCCDF_BENCHGETTER(value)   XCCDF_BENCHGETTER(result)
#undef XCCDF_BENCHGETTER

static void *xccdf_item_convert(struct xccdf_item *item, xccdf_type_t type)
{
	return (item != NULL && (item->type & type) ? item : NULL);
}

#define XCCDF_ITEM_CONVERT(T1,T2) struct xccdf_##T1* xccdf_item_to_##T1(struct xccdf_item* item) { return xccdf_item_convert(item, XCCDF_##T2); }
XCCDF_ITEM_CONVERT(benchmark, BENCHMARK)
XCCDF_ITEM_CONVERT(profile, PROFILE)
XCCDF_ITEM_CONVERT(rule, RULE)
XCCDF_ITEM_CONVERT(group, GROUP)
XCCDF_ITEM_CONVERT(value, VALUE)
XCCDF_ITEM_CONVERT(result, RESULT)

XCCDF_ABSTRACT_GETTER(xccdf_type_t, item, type, type)
XCCDF_ITEM_GETTER(const char *, id)

XCCDF_ITEM_TIGETTER(question);
XCCDF_ITEM_TIGETTER(rationale);
XCCDF_ITEM_TIGETTER(title);
XCCDF_ITEM_TIGETTER(description);

XCCDF_ITEM_GETTER(const char *, version)
XCCDF_ITEM_GETTER(const char *, cluster_id)
XCCDF_ITEM_GETTER(const char *, version_update) XCCDF_ITEM_GETTER(time_t, version_time) XCCDF_ITEM_GETTER(float, weight)
XCCDF_ITEM_GETTER(struct xccdf_item *, parent)
XCCDF_ITEM_GETTER(const char *, extends)
XCCDF_FLAG_GETTER(resolved)
XCCDF_FLAG_GETTER(hidden)
XCCDF_FLAG_GETTER(selected)
XCCDF_FLAG_GETTER(multiple)
XCCDF_FLAG_GETTER(prohibit_changes)
XCCDF_FLAG_GETTER(abstract)
XCCDF_FLAG_GETTER(interactive)
XCCDF_ITEM_SIGETTER(platforms)
XCCDF_ITEM_IGETTER(reference, references)
XCCDF_ITEM_IGETTER(warning, warnings)
XCCDF_ITEM_IGETTER(status, statuses)
XCCDF_ITERATOR_GEN_S(item) XCCDF_ITERATOR_GEN_S(status) XCCDF_ITERATOR_GEN_S(reference)
OSCAP_ITERATOR_GEN_T(struct xccdf_warning *, xccdf_warning)

XCCDF_ITEM_SETTER_SIMPLE(xccdf_numeric, weight)
XCCDF_ITEM_SETTER_SIMPLE(time_t, version_time)
XCCDF_ITEM_SETTER_STRING(version)
XCCDF_ITEM_SETTER_STRING(version_update)
XCCDF_ITEM_SETTER_STRING(extends)
XCCDF_ITEM_SETTER_STRING(cluster_id)

struct xccdf_item_iterator *xccdf_item_get_content(const struct xccdf_item *item)
{
	if (item == NULL) return NULL;
	switch (xccdf_item_get_type(item)) {
		case XCCDF_GROUP:     return xccdf_group_get_content(XGROUP(item));
		case XCCDF_BENCHMARK: return xccdf_benchmark_get_content(XBENCHMARK(item));
		default: return NULL;
	}
}

const struct oscap_string_map XCCDF_OPERATOR_MAP[] = {
	{XCCDF_OPERATOR_EQUALS, "equals"},
	{XCCDF_OPERATOR_NOT_EQUAL, "not equal"},
	{XCCDF_OPERATOR_GREATER, "greater than"},
	{XCCDF_OPERATOR_GREATER_EQUAL, "greater than or equal"},
	{XCCDF_OPERATOR_LESS, "less than"},
	{XCCDF_OPERATOR_LESS_EQUAL, "less than or equal"},
	{XCCDF_OPERATOR_PATTERN_MATCH, "pattern match"},
	{0, NULL}
};

const struct oscap_string_map XCCDF_LEVEL_MAP[] = {
	{XCCDF_UNKNOWN, "unknown"},
	{XCCDF_INFO, "info"},
	{XCCDF_LOW, "low"},
	{XCCDF_MEDIUM, "medium"},
	{XCCDF_HIGH, "high"},
	{0, NULL}
};

static const struct oscap_string_map XCCDF_STATUS_MAP[] = {
	{XCCDF_STATUS_ACCEPTED, "accepted"},
	{XCCDF_STATUS_DEPRECATED, "deprecated"},
	{XCCDF_STATUS_DRAFT, "draft"},
	{XCCDF_STATUS_INCOMPLETE, "incomplete"},
	{XCCDF_STATUS_INTERIM, "interim"},
	{XCCDF_STATUS_NOT_SPECIFIED, NULL}
};

struct xccdf_status *xccdf_status_new(const char *status, const char *date)
{
	struct xccdf_status *ret;
	if (!status)
		return NULL;
	ret = oscap_calloc(1, sizeof(struct xccdf_status));
	if ((ret->status = oscap_string_to_enum(XCCDF_STATUS_MAP, status)) == XCCDF_STATUS_NOT_SPECIFIED) {
		oscap_free(ret);
		return NULL;
	}
	ret->date = oscap_get_date(date);
	return ret;
}

void xccdf_status_dump(struct xccdf_status *status, int depth)
{
	xccdf_print_depth(depth);
	time_t date = xccdf_status_get_date(status);
	printf("%-10s (%24.24s)\n", oscap_enum_to_string(XCCDF_STATUS_MAP, xccdf_status_get_status(status)),
	       (date ? ctime(&date) : "   date not specified   "));
}

void xccdf_status_free(struct xccdf_status *status)
{
	oscap_free(status);
}

XCCDF_GENERIC_GETTER(time_t, status, date)
    XCCDF_GENERIC_GETTER(xccdf_status_type_t, status, status)

xccdf_status_type_t xccdf_item_get_current_status(const struct xccdf_item *item)
{
	time_t maxtime = 0;
	xccdf_status_type_t maxtype = XCCDF_STATUS_NOT_SPECIFIED;
	const struct oscap_list_item *li = item->item.statuses->first;
	struct xccdf_status *status;
	while (li) {
		status = li->data;
		if (status->date == 0 || status->date >= maxtime) {
			maxtime = status->date;
			maxtype = status->status;
		}
		li = li->next;
	}
	return maxtype;
}

struct xccdf_model *xccdf_model_new_xml(xmlTextReaderPtr reader)
{
	xccdf_element_t el = xccdf_element_get(reader);
	int depth = oscap_element_depth(reader) + 1;
	struct xccdf_model *model;

	if (el != XCCDFE_MODEL)
		return NULL;

	model = oscap_calloc(1, sizeof(struct xccdf_model));
	model->system = xccdf_attribute_copy(reader, XCCDFA_SYSTEM);
	model->params = oscap_htable_new();

	while (oscap_to_start_element(reader, depth)) {
		if (xccdf_element_get(reader) == XCCDFE_PARAM) {
			const char *name = xccdf_attribute_get(reader, XCCDFA_NAME);
			char *value = oscap_element_string_copy(reader);
			if (!name || !value || !oscap_htable_add(model->params, name, value))
				oscap_free(value);
		}
	}

	return model;
}

XCCDF_GENERIC_GETTER(const char *, model, system)

static const struct oscap_string_map XCCDF_WARNING_MAP[] = {
	{ XCCDF_WARNING_GENERAL, "general" },
	{ XCCDF_WARNING_FUNCTIONALITY, "functionality" },
	{ XCCDF_WARNING_PERFORMANCE, "performance" },
	{ XCCDF_WARNING_HARDWARE, "hardware" },
	{ XCCDF_WARNING_LEGAL, "legal" },
	{ XCCDF_WARNING_REGULATORY, "regulatory" },
	{ XCCDF_WARNING_MANAGEMENT, "management" },
	{ XCCDF_WARNING_AUDIT, "audit" },
	{ XCCDF_WARNING_DEPENDENCY, "dependency" },
	{ XCCDF_WARNING_GENERAL, NULL }
};

void xccdf_model_free(struct xccdf_model *model)
{
	if (model) {
		oscap_free(model->system);
		oscap_htable_free(model->params, oscap_free);
		oscap_free(model);
	}
}

void xccdf_cstring_dump(const char *data, int depth)
{
	xccdf_print_depth(depth);
	printf("%s\n", data);
}

struct xccdf_warning *xccdf_warning_new(void)
{
    struct xccdf_warning *w = oscap_calloc(1, sizeof(struct xccdf_warning));
    w->category = XCCDF_WARNING_GENERAL;
    return w;
}

struct xccdf_warning *xccdf_warning_new_parse(xmlTextReaderPtr reader)
{
    struct xccdf_warning *w = xccdf_warning_new();
    w->text = oscap_text_new_parse(XCCDF_TEXT_HTMLSUB, reader);
    w->category = oscap_string_to_enum(XCCDF_WARNING_MAP, xccdf_attribute_get(reader, XCCDFA_CATEGORY));
    return w;
}

void xccdf_warning_free(struct xccdf_warning * w)
{
    if (w != NULL) {
        oscap_text_free(w->text);
        oscap_free(w);
    }
}

OSCAP_GETTER(xccdf_warning_category_t, xccdf_warning, category)
OSCAP_GETTER(struct oscap_text *, xccdf_warning, text)

struct xccdf_reference *xccdf_reference_new(void)
{
    struct xccdf_reference *ref = oscap_calloc(1, sizeof(struct xccdf_reference));
    return ref;
}

struct xccdf_reference *xccdf_reference_new_parse(xmlTextReaderPtr reader)
{
    struct xccdf_reference *ref = xccdf_reference_new();
    if (xccdf_attribute_has(reader, XCCDFA_OVERRIDE))
        ref->override = oscap_string_to_enum(OSCAP_BOOL_MAP, xccdf_attribute_get(reader, XCCDFA_OVERRIDE));
    ref->href = xccdf_attribute_copy(reader, XCCDFA_HREF);
    ref->content = oscap_element_string_copy(reader);
    // TODO lang, Dublin Core
    return ref;
}

void xccdf_reference_free(struct xccdf_reference *ref)
{
    if (ref != NULL) {
        oscap_free(ref->lang);
        oscap_free(ref->href);
        oscap_free(ref->content);
        oscap_free(ref);
    }
}

OSCAP_GETTER(const char*, xccdf_reference, lang)
OSCAP_GETTER(const char*, xccdf_reference, href)
OSCAP_GETTER(const char*, xccdf_reference, content)
OSCAP_GETTER(bool,        xccdf_reference, override)

const struct oscap_text_traits XCCDF_TEXT_PLAIN    = { .can_override = true };
const struct oscap_text_traits XCCDF_TEXT_HTML     = { .html = true, .can_override = true };
const struct oscap_text_traits XCCDF_TEXT_PLAINSUB = { .can_override = true, .can_substitute = true };
const struct oscap_text_traits XCCDF_TEXT_HTMLSUB  = { .html = true, .can_override = true, .can_substitute = true };
const struct oscap_text_traits XCCDF_TEXT_NOTICE   = { .html = true };
const struct oscap_text_traits XCCDF_TEXT_PROFNOTE = { .html = true, .can_substitute = true };

