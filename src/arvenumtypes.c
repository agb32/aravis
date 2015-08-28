
/* Generated data (by glib-mkenums) */

#include "arvenumtypes.h"
#include "arvenums.h"

GType
arv_gc_name_space_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_NAME_SPACE_STANDARD,
			  "ARV_GC_NAME_SPACE_STANDARD",
			  "standard" },
			{ ARV_GC_NAME_SPACE_CUSTOM,
			  "ARV_GC_NAME_SPACE_CUSTOM",
			  "custom" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcNameSpace"),
				values);
	}
	return the_type;
}

GType
arv_gc_access_mode_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_ACCESS_MODE_RO,
			  "ARV_GC_ACCESS_MODE_RO",
			  "ro" },
			{ ARV_GC_ACCESS_MODE_WO,
			  "ARV_GC_ACCESS_MODE_WO",
			  "wo" },
			{ ARV_GC_ACCESS_MODE_RW,
			  "ARV_GC_ACCESS_MODE_RW",
			  "rw" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcAccessMode"),
				values);
	}
	return the_type;
}

GType
arv_gc_cachable_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_CACHABLE_NO_CACHE,
			  "ARV_GC_CACHABLE_NO_CACHE",
			  "no-cache" },
			{ ARV_GC_CACHABLE_WRITE_TRHOUGH,
			  "ARV_GC_CACHABLE_WRITE_TRHOUGH",
			  "write-trhough" },
			{ ARV_GC_CACHABLE_WRITE_AROUND,
			  "ARV_GC_CACHABLE_WRITE_AROUND",
			  "write-around" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcCachable"),
				values);
	}
	return the_type;
}

GType
arv_auto_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_AUTO_OFF,
			  "ARV_AUTO_OFF",
			  "off" },
			{ ARV_AUTO_ONCE,
			  "ARV_AUTO_ONCE",
			  "once" },
			{ ARV_AUTO_CONTINUOUS,
			  "ARV_AUTO_CONTINUOUS",
			  "continuous" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvAuto"),
				values);
	}
	return the_type;
}

GType
arv_acquisition_mode_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_ACQUISITION_MODE_CONTINUOUS,
			  "ARV_ACQUISITION_MODE_CONTINUOUS",
			  "continuous" },
			{ ARV_ACQUISITION_MODE_SINGLE_FRAME,
			  "ARV_ACQUISITION_MODE_SINGLE_FRAME",
			  "single-frame" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvAcquisitionMode"),
				values);
	}
	return the_type;
}

#include "arvdebug.h"

GType
arv_debug_level_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_DEBUG_LEVEL_NONE,
			  "ARV_DEBUG_LEVEL_NONE",
			  "none" },
			{ ARV_DEBUG_LEVEL_WARNING,
			  "ARV_DEBUG_LEVEL_WARNING",
			  "warning" },
			{ ARV_DEBUG_LEVEL_DEBUG,
			  "ARV_DEBUG_LEVEL_DEBUG",
			  "debug" },
			{ ARV_DEBUG_LEVEL_LOG,
			  "ARV_DEBUG_LEVEL_LOG",
			  "log" },
			{ ARV_DEBUG_LEVEL_COUNT,
			  "ARV_DEBUG_LEVEL_COUNT",
			  "count" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvDebugLevel"),
				values);
	}
	return the_type;
}

#include "arvdomnode.h"

GType
arv_dom_node_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_DOM_NODE_TYPE_ELEMENT_NODE,
			  "ARV_DOM_NODE_TYPE_ELEMENT_NODE",
			  "element-node" },
			{ ARV_DOM_NODE_TYPE_ATTRIBUTE_NODE,
			  "ARV_DOM_NODE_TYPE_ATTRIBUTE_NODE",
			  "attribute-node" },
			{ ARV_DOM_NODE_TYPE_TEXT_NODE,
			  "ARV_DOM_NODE_TYPE_TEXT_NODE",
			  "text-node" },
			{ ARV_DOM_NODE_TYPE_CDATA_SECTION_NODE,
			  "ARV_DOM_NODE_TYPE_CDATA_SECTION_NODE",
			  "cdata-section-node" },
			{ ARV_DOM_NODE_TYPE_ENTITY_REFERENCE_NODE,
			  "ARV_DOM_NODE_TYPE_ENTITY_REFERENCE_NODE",
			  "entity-reference-node" },
			{ ARV_DOM_NODE_TYPE_ENTITY_NODE,
			  "ARV_DOM_NODE_TYPE_ENTITY_NODE",
			  "entity-node" },
			{ ARV_DOM_NODE_TYPE_PROCESSING_INSTRUCTION_NODE,
			  "ARV_DOM_NODE_TYPE_PROCESSING_INSTRUCTION_NODE",
			  "processing-instruction-node" },
			{ ARV_DOM_NODE_TYPE_COMMENT_NODE,
			  "ARV_DOM_NODE_TYPE_COMMENT_NODE",
			  "comment-node" },
			{ ARV_DOM_NODE_TYPE_DOCUMENT_NODE,
			  "ARV_DOM_NODE_TYPE_DOCUMENT_NODE",
			  "document-node" },
			{ ARV_DOM_NODE_TYPE_DOCUMENT_TYPE_NODE,
			  "ARV_DOM_NODE_TYPE_DOCUMENT_TYPE_NODE",
			  "document-type-node" },
			{ ARV_DOM_NODE_TYPE_DOCUMENT_FRAGMENT_NODE,
			  "ARV_DOM_NODE_TYPE_DOCUMENT_FRAGMENT_NODE",
			  "document-fragment-node" },
			{ ARV_DOM_NODE_TYPE_NOTATION_NODE,
			  "ARV_DOM_NODE_TYPE_NOTATION_NODE",
			  "notation-node" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvDomNodeType"),
				values);
	}
	return the_type;
}

#include "arvgcpropertynode.h"

GType
arv_gc_property_node_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_PROPERTY_NODE_TYPE_UNKNOWN,
			  "ARV_GC_PROPERTY_NODE_TYPE_UNKNOWN",
			  "unknown" },
			{ ARV_GC_PROPERTY_NODE_TYPE_VALUE,
			  "ARV_GC_PROPERTY_NODE_TYPE_VALUE",
			  "value" },
			{ ARV_GC_PROPERTY_NODE_TYPE_ADDRESS,
			  "ARV_GC_PROPERTY_NODE_TYPE_ADDRESS",
			  "address" },
			{ ARV_GC_PROPERTY_NODE_TYPE_DESCRIPTION,
			  "ARV_GC_PROPERTY_NODE_TYPE_DESCRIPTION",
			  "description" },
			{ ARV_GC_PROPERTY_NODE_TYPE_TOOLTIP,
			  "ARV_GC_PROPERTY_NODE_TYPE_TOOLTIP",
			  "tooltip" },
			{ ARV_GC_PROPERTY_NODE_TYPE_DISPLAY_NAME,
			  "ARV_GC_PROPERTY_NODE_TYPE_DISPLAY_NAME",
			  "display-name" },
			{ ARV_GC_PROPERTY_NODE_TYPE_MINIMUM,
			  "ARV_GC_PROPERTY_NODE_TYPE_MINIMUM",
			  "minimum" },
			{ ARV_GC_PROPERTY_NODE_TYPE_MAXIMUM,
			  "ARV_GC_PROPERTY_NODE_TYPE_MAXIMUM",
			  "maximum" },
			{ ARV_GC_PROPERTY_NODE_TYPE_INCREMENT,
			  "ARV_GC_PROPERTY_NODE_TYPE_INCREMENT",
			  "increment" },
			{ ARV_GC_PROPERTY_NODE_TYPE_UNIT,
			  "ARV_GC_PROPERTY_NODE_TYPE_UNIT",
			  "unit" },
			{ ARV_GC_PROPERTY_NODE_TYPE_ON_VALUE,
			  "ARV_GC_PROPERTY_NODE_TYPE_ON_VALUE",
			  "on-value" },
			{ ARV_GC_PROPERTY_NODE_TYPE_OFF_VALUE,
			  "ARV_GC_PROPERTY_NODE_TYPE_OFF_VALUE",
			  "off-value" },
			{ ARV_GC_PROPERTY_NODE_TYPE_LENGTH,
			  "ARV_GC_PROPERTY_NODE_TYPE_LENGTH",
			  "length" },
			{ ARV_GC_PROPERTY_NODE_TYPE_FORMULA,
			  "ARV_GC_PROPERTY_NODE_TYPE_FORMULA",
			  "formula" },
			{ ARV_GC_PROPERTY_NODE_TYPE_FORMULA_TO,
			  "ARV_GC_PROPERTY_NODE_TYPE_FORMULA_TO",
			  "formula-to" },
			{ ARV_GC_PROPERTY_NODE_TYPE_FORMULA_FROM,
			  "ARV_GC_PROPERTY_NODE_TYPE_FORMULA_FROM",
			  "formula-from" },
			{ ARV_GC_PROPERTY_NODE_TYPE_EXPRESSION,
			  "ARV_GC_PROPERTY_NODE_TYPE_EXPRESSION",
			  "expression" },
			{ ARV_GC_PROPERTY_NODE_TYPE_CONSTANT,
			  "ARV_GC_PROPERTY_NODE_TYPE_CONSTANT",
			  "constant" },
			{ ARV_GC_PROPERTY_NODE_TYPE_ACCESS_MODE,
			  "ARV_GC_PROPERTY_NODE_TYPE_ACCESS_MODE",
			  "access-mode" },
			{ ARV_GC_PROPERTY_NODE_TYPE_CACHABLE,
			  "ARV_GC_PROPERTY_NODE_TYPE_CACHABLE",
			  "cachable" },
			{ ARV_GC_PROPERTY_NODE_TYPE_POLLING_TIME,
			  "ARV_GC_PROPERTY_NODE_TYPE_POLLING_TIME",
			  "polling-time" },
			{ ARV_GC_PROPERTY_NODE_TYPE_ENDIANESS,
			  "ARV_GC_PROPERTY_NODE_TYPE_ENDIANESS",
			  "endianess" },
			{ ARV_GC_PROPERTY_NODE_TYPE_SIGN,
			  "ARV_GC_PROPERTY_NODE_TYPE_SIGN",
			  "sign" },
			{ ARV_GC_PROPERTY_NODE_TYPE_LSB,
			  "ARV_GC_PROPERTY_NODE_TYPE_LSB",
			  "lsb" },
			{ ARV_GC_PROPERTY_NODE_TYPE_MSB,
			  "ARV_GC_PROPERTY_NODE_TYPE_MSB",
			  "msb" },
			{ ARV_GC_PROPERTY_NODE_TYPE_BIT,
			  "ARV_GC_PROPERTY_NODE_TYPE_BIT",
			  "bit" },
			{ ARV_GC_PROPERTY_NODE_TYPE_COMMAND_VALUE,
			  "ARV_GC_PROPERTY_NODE_TYPE_COMMAND_VALUE",
			  "command-value" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_UNKNONW,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_UNKNONW",
			  "p-unknonw" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_FEATURE,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_FEATURE",
			  "p-feature" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_VALUE,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_VALUE",
			  "p-value" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_ADDRESS,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_ADDRESS",
			  "p-address" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_IS_IMPLEMENTED,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_IS_IMPLEMENTED",
			  "p-is-implemented" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_IS_LOCKED,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_IS_LOCKED",
			  "p-is-locked" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_IS_AVAILABLE,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_IS_AVAILABLE",
			  "p-is-available" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_MINIMUM,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_MINIMUM",
			  "p-minimum" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_MAXIMUM,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_MAXIMUM",
			  "p-maximum" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_INCREMENT,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_INCREMENT",
			  "p-increment" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_INDEX,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_INDEX",
			  "p-index" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_LENGTH,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_LENGTH",
			  "p-length" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_PORT,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_PORT",
			  "p-port" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_VARIABLE,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_VARIABLE",
			  "p-variable" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_INVALIDATOR,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_INVALIDATOR",
			  "p-invalidator" },
			{ ARV_GC_PROPERTY_NODE_TYPE_P_COMMAND_VALUE,
			  "ARV_GC_PROPERTY_NODE_TYPE_P_COMMAND_VALUE",
			  "p-command-value" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcPropertyNodeType"),
				values);
	}
	return the_type;
}

#include "arvgcregisternode.h"

GType
arv_gc_sign_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_SIGN_SIGNED,
			  "ARV_GC_SIGN_SIGNED",
			  "signed" },
			{ ARV_GC_SIGN_UNSIGNED,
			  "ARV_GC_SIGN_UNSIGNED",
			  "unsigned" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcSign"),
				values);
	}
	return the_type;
}

GType
arv_gc_register_node_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_REGISTER_NODE_TYPE_REGISTER,
			  "ARV_GC_REGISTER_NODE_TYPE_REGISTER",
			  "register" },
			{ ARV_GC_REGISTER_NODE_TYPE_INTEGER,
			  "ARV_GC_REGISTER_NODE_TYPE_INTEGER",
			  "integer" },
			{ ARV_GC_REGISTER_NODE_TYPE_MASKED_INTEGER,
			  "ARV_GC_REGISTER_NODE_TYPE_MASKED_INTEGER",
			  "masked-integer" },
			{ ARV_GC_REGISTER_NODE_TYPE_FLOAT,
			  "ARV_GC_REGISTER_NODE_TYPE_FLOAT",
			  "float" },
			{ ARV_GC_REGISTER_NODE_TYPE_STRING,
			  "ARV_GC_REGISTER_NODE_TYPE_STRING",
			  "string" },
			{ ARV_GC_REGISTER_NODE_TYPE_STRUCT_REGISTER,
			  "ARV_GC_REGISTER_NODE_TYPE_STRUCT_REGISTER",
			  "struct-register" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcRegisterNodeType"),
				values);
	}
	return the_type;
}

#include "arvdevice.h"

GType
arv_device_status_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_DEVICE_STATUS_UNKNOWN,
			  "ARV_DEVICE_STATUS_UNKNOWN",
			  "unknown" },
			{ ARV_DEVICE_STATUS_SUCCESS,
			  "ARV_DEVICE_STATUS_SUCCESS",
			  "success" },
			{ ARV_DEVICE_STATUS_TIMEOUT,
			  "ARV_DEVICE_STATUS_TIMEOUT",
			  "timeout" },
			{ ARV_DEVICE_STATUS_WRITE_ERROR,
			  "ARV_DEVICE_STATUS_WRITE_ERROR",
			  "write-error" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvDeviceStatus"),
				values);
	}
	return the_type;
}

#include "arvstream.h"

GType
arv_stream_callback_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_STREAM_CALLBACK_TYPE_INIT,
			  "ARV_STREAM_CALLBACK_TYPE_INIT",
			  "init" },
			{ ARV_STREAM_CALLBACK_TYPE_EXIT,
			  "ARV_STREAM_CALLBACK_TYPE_EXIT",
			  "exit" },
			{ ARV_STREAM_CALLBACK_TYPE_START_BUFFER,
			  "ARV_STREAM_CALLBACK_TYPE_START_BUFFER",
			  "start-buffer" },
			{ ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE,
			  "ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE",
			  "buffer-done" },
			{ ARV_STREAM_CALLBACK_TYPE_NEW_DATA,
			  "ARV_STREAM_CALLBACK_TYPE_NEW_DATA",
			  "new-data" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvStreamCallbackType"),
				values);
	}
	return the_type;
}

#include "arvbuffer.h"

GType
arv_buffer_status_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_BUFFER_STATUS_SUCCESS,
			  "ARV_BUFFER_STATUS_SUCCESS",
			  "success" },
			{ ARV_BUFFER_STATUS_CLEARED,
			  "ARV_BUFFER_STATUS_CLEARED",
			  "cleared" },
			{ ARV_BUFFER_STATUS_TIMEOUT,
			  "ARV_BUFFER_STATUS_TIMEOUT",
			  "timeout" },
			{ ARV_BUFFER_STATUS_MISSING_PACKETS,
			  "ARV_BUFFER_STATUS_MISSING_PACKETS",
			  "missing-packets" },
			{ ARV_BUFFER_STATUS_WRONG_PACKET_ID,
			  "ARV_BUFFER_STATUS_WRONG_PACKET_ID",
			  "wrong-packet-id" },
			{ ARV_BUFFER_STATUS_SIZE_MISMATCH,
			  "ARV_BUFFER_STATUS_SIZE_MISMATCH",
			  "size-mismatch" },
			{ ARV_BUFFER_STATUS_FILLING,
			  "ARV_BUFFER_STATUS_FILLING",
			  "filling" },
			{ ARV_BUFFER_STATUS_ABORTED,
			  "ARV_BUFFER_STATUS_ABORTED",
			  "aborted" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvBufferStatus"),
				values);
	}
	return the_type;
}

#include "arvgvstream.h"

GType
arv_gv_stream_socket_buffer_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GV_STREAM_SOCKET_BUFFER_FIXED,
			  "ARV_GV_STREAM_SOCKET_BUFFER_FIXED",
			  "fixed" },
			{ ARV_GV_STREAM_SOCKET_BUFFER_AUTO,
			  "ARV_GV_STREAM_SOCKET_BUFFER_AUTO",
			  "auto" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvStreamSocketBuffer"),
				values);
	}
	return the_type;
}

GType
arv_gv_stream_packet_resend_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GV_STREAM_PACKET_RESEND_NEVER,
			  "ARV_GV_STREAM_PACKET_RESEND_NEVER",
			  "never" },
			{ ARV_GV_STREAM_PACKET_RESEND_ALWAYS,
			  "ARV_GV_STREAM_PACKET_RESEND_ALWAYS",
			  "always" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvStreamPacketResend"),
				values);
	}
	return the_type;
}

#include "arvgvcp.h"

GType
arv_gvcp_packet_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GVCP_PACKET_TYPE_ACK,
			  "ARV_GVCP_PACKET_TYPE_ACK",
			  "ack" },
			{ ARV_GVCP_PACKET_TYPE_RESEND,
			  "ARV_GVCP_PACKET_TYPE_RESEND",
			  "resend" },
			{ ARV_GVCP_PACKET_TYPE_CMD,
			  "ARV_GVCP_PACKET_TYPE_CMD",
			  "cmd" },
			{ ARV_GVCP_PACKET_TYPE_ERROR,
			  "ARV_GVCP_PACKET_TYPE_ERROR",
			  "error" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvcpPacketType"),
				values);
	}
	return the_type;
}

GType
arv_gvcp_command_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GVCP_COMMAND_DISCOVERY_CMD,
			  "ARV_GVCP_COMMAND_DISCOVERY_CMD",
			  "discovery-cmd" },
			{ ARV_GVCP_COMMAND_DISCOVERY_ACK,
			  "ARV_GVCP_COMMAND_DISCOVERY_ACK",
			  "discovery-ack" },
			{ ARV_GVCP_COMMAND_BYE_CMD,
			  "ARV_GVCP_COMMAND_BYE_CMD",
			  "bye-cmd" },
			{ ARV_GVCP_COMMAND_BYE_ACK,
			  "ARV_GVCP_COMMAND_BYE_ACK",
			  "bye-ack" },
			{ ARV_GVCP_COMMAND_PACKET_RESEND_CMD,
			  "ARV_GVCP_COMMAND_PACKET_RESEND_CMD",
			  "packet-resend-cmd" },
			{ ARV_GVCP_COMMAND_PACKET_RESEND_ACK,
			  "ARV_GVCP_COMMAND_PACKET_RESEND_ACK",
			  "packet-resend-ack" },
			{ ARV_GVCP_COMMAND_READ_REGISTER_CMD,
			  "ARV_GVCP_COMMAND_READ_REGISTER_CMD",
			  "read-register-cmd" },
			{ ARV_GVCP_COMMAND_READ_REGISTER_ACK,
			  "ARV_GVCP_COMMAND_READ_REGISTER_ACK",
			  "read-register-ack" },
			{ ARV_GVCP_COMMAND_WRITE_REGISTER_CMD,
			  "ARV_GVCP_COMMAND_WRITE_REGISTER_CMD",
			  "write-register-cmd" },
			{ ARV_GVCP_COMMAND_WRITE_REGISTER_ACK,
			  "ARV_GVCP_COMMAND_WRITE_REGISTER_ACK",
			  "write-register-ack" },
			{ ARV_GVCP_COMMAND_READ_MEMORY_CMD,
			  "ARV_GVCP_COMMAND_READ_MEMORY_CMD",
			  "read-memory-cmd" },
			{ ARV_GVCP_COMMAND_READ_MEMORY_ACK,
			  "ARV_GVCP_COMMAND_READ_MEMORY_ACK",
			  "read-memory-ack" },
			{ ARV_GVCP_COMMAND_WRITE_MEMORY_CMD,
			  "ARV_GVCP_COMMAND_WRITE_MEMORY_CMD",
			  "write-memory-cmd" },
			{ ARV_GVCP_COMMAND_WRITE_MEMORY_ACK,
			  "ARV_GVCP_COMMAND_WRITE_MEMORY_ACK",
			  "write-memory-ack" },
			{ ARV_GVCP_COMMAND_PENDING_ACK,
			  "ARV_GVCP_COMMAND_PENDING_ACK",
			  "pending-ack" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvcpCommand"),
				values);
	}
	return the_type;
}

#include "arvgvsp.h"

GType
arv_gvsp_packet_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GVSP_PACKET_TYPE_OK,
			  "ARV_GVSP_PACKET_TYPE_OK",
			  "ok" },
			{ ARV_GVSP_PACKET_TYPE_RESEND,
			  "ARV_GVSP_PACKET_TYPE_RESEND",
			  "resend" },
			{ ARV_GVSP_PACKET_TYPE_ERROR,
			  "ARV_GVSP_PACKET_TYPE_ERROR",
			  "error" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvspPacketType"),
				values);
	}
	return the_type;
}

GType
arv_gvsp_content_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GVSP_CONTENT_TYPE_DATA_LEADER,
			  "ARV_GVSP_CONTENT_TYPE_DATA_LEADER",
			  "leader" },
			{ ARV_GVSP_CONTENT_TYPE_DATA_TRAILER,
			  "ARV_GVSP_CONTENT_TYPE_DATA_TRAILER",
			  "trailer" },
			{ ARV_GVSP_CONTENT_TYPE_DATA_BLOCK,
			  "ARV_GVSP_CONTENT_TYPE_DATA_BLOCK",
			  "block" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvspContentType"),
				values);
	}
	return the_type;
}


/* Generated data ends here */

