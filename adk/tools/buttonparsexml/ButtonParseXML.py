import xml.etree.ElementTree as ET
import os
import sys

class Pio:
    def __init__(self, element):
        self.name = element.find("pinFriendlyName").text
        self.pad = int(element.find("pad").text)
        self.input_event_id = -1

    def __repr__(self):
        return str(self.name) + " at PIO " + str(self.pad)

    def set_input_event_id(self, id):
        self.input_event_id = id

class PioDebounce:
    def __init__(self, element = None):
        self.num_reads = 4
        self.period = 5
        if element != None:        
            _nreads = element.find("nreads").text
            if _nreads != None:
                self.num_reads = int(_nreads)
            _period = element.find("period").text
            if _period != None:
                self.period = int(_period)
    
class Message:
    def __init__(self, element, pio_dict):
        self.device_specific = element.get("device_specific", default="false")

        self.name = element.find("messageName").text
        self.event = element.find("buttonEvent").text

        _timeout_ms = element.find("timeout_ms")
        if _timeout_ms != None:
            self.timeout_ms = int(_timeout_ms.text)
        else:
            self.timeout_ms = 0

        _repeat_ms = element.find("repeat_ms")
        if _repeat_ms != None:
            self.repeat_ms = int(_repeat_ms.text)
        else:
            self.repeat_ms = 0

        self.active_hi_pio_list = []
        for _active_hi_pio in element.findall('activePinFriendlyName'):
            self.active_hi_pio_list.append(pio_dict[_active_hi_pio.text])

        self.active_lo_pio_list = []
        for _active_lo_pio in element.findall('negatePinFriendlyName'):
            self.active_lo_pio_list.append(pio_dict[_active_lo_pio.text])



class XmlValidationException(Exception):
    def __init__(self, message):
        self.message = 'XmlValidationException: ' + message


def AutoValidateXML(xsd, xml):
    'Perform auto-validation of the xml'
    try:
        from lxml import etree
    except ImportError:
        sys.stderr.write("Skipping XML schema validation, lxml library not available\n")
    else:
        xmlschema_doc = etree.parse(xsd)
        xmlschema = etree.XMLSchema(xmlschema_doc)
        try:
            doc = etree.parse(xml)
            xmlschema.assertValid(doc)
        except:
            raise XmlValidationException('(line %d): ' % xmlschema.error_log.last_error.line + xmlschema.error_log.last_error.message)

def ParsePioDebounce(xml):
    tree = ET.parse(xml)
    root = tree.getroot()

    element = root.find("pio_debounce")
    return PioDebounce(element)

def ParsePioXml(xml):
    pio_dict = dict()
    tree = ET.parse(xml)
    root = tree.getroot()

    for element in root.findall("pio"):
        pio = Pio(element)
        pio.set_input_event_id(len(pio_dict))
        pio_dict[pio.name] = pio

    return pio_dict

def ParseMessageXml(xml, pio_dict):
    message_group_dict = dict()
    tree = ET.parse(xml)
    root = tree.getroot()

    for message_group in root.findall("message_group"):
        message_dict = dict()
        message_group_dict[message_group.attrib["name"]] = message_dict

        for element in message_group.findall("message"):
            message = Message(element, pio_dict)
            message_dict[message.name] = message

    return message_group_dict


def GetNumberOfNotIgnoredMessages(message_dict):
    'Returns number of messages less number of ignored messages'
    messages_to_ignore = sum(value.event == "IGNORE" for value in message_dict.values())
    return len(message_dict) - messages_to_ignore

def OutputMessageGroups(message_group_dict):
    message_groups_str = ""
    for message_group_name, message_dict in message_group_dict.iteritems():
        message_groups_str += "extern const InputActionMessage_t " + message_group_name + "[" + str(GetNumberOfNotIgnoredMessages(message_dict)) + "];\n"
    return message_groups_str + "\n"

def OutputInputDefines(pio_dict):
    define_str = ""
    for pio in pio_dict.itervalues():
        define_str += "#define " + format(pio.name, "20s") + " (1UL << " + format(pio.input_event_id, "2d") + ")\n"
    return define_str

def OutputMessageIds(message_group_dict):
    device_specific_messages = set()
    non_device_specific_messages = set()

    for message_group in message_group_dict.itervalues():
        for msg in message_group.itervalues():
            if msg.device_specific == "true" or msg.device_specific == "1":
                device_specific_messages.add(msg.name)
            else:
                non_device_specific_messages.add(msg.name)

    device_specific_message_id = len(device_specific_messages)
    non_device_specific_message_id = 0

    message_str = "#define MIN_INPUT_ACTION_MESSAGE_ID (LOGICAL_INPUT_MESSAGE_BASE-" + format(device_specific_message_id, "d") + ")\n"
    message_str += "#define MAX_INPUT_ACTION_MESSAGE_ID (LOGICAL_INPUT_MESSAGE_BASE+" + format(non_device_specific_message_id + len(non_device_specific_messages) - 1, "d") + ")\n\n"

    for message in device_specific_messages:
        message_str += "#define " + format(message, "40s") + " (LOGICAL_INPUT_MESSAGE_BASE-" + format(device_specific_message_id, "#4x") + ")\n"
        device_specific_message_id -= 1

    for message in non_device_specific_messages:
        message_str += "#define " + format(message, "40s") + " (LOGICAL_INPUT_MESSAGE_BASE+" + format(non_device_specific_message_id, "#4x") + ")\n"
        non_device_specific_message_id += 1

    return message_str

def OutputPioConfigTable(pio_debounce, pio_dict, num_pio_bank):

    # Create array of all possible PIOs, populate with PIOs in use
    # 32 pio to a bank 
    pio_list = [-1] * num_pio_bank * 32
    for k,v in pio_dict.items():
        pio_list[v.pad] = v.input_event_id

    table_str = ""

    # Output pio mapping table
    table_str += "\t/* Table to convert from PIO to input event ID*/\n\t{\n\t\t"
    for idx, pio in enumerate(pio_list):
        table_str += format(pio, "2d")
        if idx < (len(pio_list) - 1):
            table_str += ", "
            if idx % 16 == 15:
                table_str += "\n\t\t"
    table_str += "\n\t},\n"

    # Output pio configuration table
    pio_mask = 0L
    for pio in pio_dict.itervalues():
        pio_mask |= (1L << pio.pad)

    bank_str = "\t/* Masks for each PIO bank to configure as inputs */\n\t{ "
    for bank in range(0, num_pio_bank):
        bank_str += "0x" + format(pio_mask >> (bank * 32) & 0xFFFFFFFF, "08x") + "UL"
        if bank < (num_pio_bank - 1):
            bank_str += ", "
    bank_str += " },\n"

    debounce_str = "\t/* PIO debounce settings */\n\t" + str(pio_debounce.num_reads) + ", " + str(pio_debounce.period) + "\n"

    return "const InputEventConfig_t input_event_config = \n{\n" + table_str + "\n" + bank_str + debounce_str + "};\n"


def OutputButtonTable(message_group_dict):

    def OutputButtonAction(message_dict, action):
        button_str = ""
        for v in message_dict.itervalues():
            if v.event == action:
                button_str += "\t{\n"
                pio_list = v.active_hi_pio_list
                pio_str = "0"
                for idx, pio in enumerate(pio_list):
                    if idx > 0:
                        pio_str += " | " + pio.name
                    else:
                        pio_str = pio.name
                pio_str += ","
                button_str += "\t\t" + format(pio_str, "40s") + "/* Input event bits */\n"
                pio_list = v.active_hi_pio_list + v.active_lo_pio_list
                pio_str = ""
                for idx, pio in enumerate(pio_list):
                    if idx > 0:
                        pio_str += " | "
                    pio_str += pio.name
                pio_str += ","
                button_str += "\t\t" + format(pio_str, "40s") + "/* Input event mask */\n"
                button_str += "\t\t" + format(v.event + ",", "40s") + "/* Action */\n"
                button_str += "\t\t" + format(str(v.timeout_ms) + ",", "40s") + "/* Timeout */\n"
                button_str += "\t\t" + format(str(v.repeat_ms) + ",", "40s") + "/* Repeat */\n"
                button_str += "\t\t" + format(v.name + ",", "40s") + "/* Message */\n"
                button_str += "\t},\n"
        return button_str

    table_str = ""
    for message_group_name, message_dict in message_group_dict.iteritems():
        table_str += "\nconst InputActionMessage_t " + message_group_name + "[" + str(GetNumberOfNotIgnoredMessages(message_dict)) + "] = \n"

        table_str += "{\n"
        table_str += OutputButtonAction(message_dict, "ENTER")
        table_str += OutputButtonAction(message_dict, "HELD")
        table_str += OutputButtonAction(message_dict, "SINGLE_CLICK")
        table_str += OutputButtonAction(message_dict, "HELD_RELEASE")
        table_str += OutputButtonAction(message_dict, "DOUBLE_CLICK")
        table_str += OutputButtonAction(message_dict, "RELEASE")
        table_str += "};\n"

    return table_str

def generate_header(args, pio_dict, message_group_dict):
    h_file = "#ifndef BUTTON_CONFIG_H\n#define BUTTON_CONFIG_H\n\n" + \
             "#include \"domain_message.h\"\n" + \
             "#include \"input_event_manager.h\"\n" + \
             "extern const InputEventConfig_t input_event_config;\n\n" + \
             OutputMessageGroups(message_group_dict) + \
             OutputInputDefines(pio_dict) + "\n" + \
             OutputMessageIds(message_group_dict) + \
             "\n#endif\n"
    print h_file

def generate_source(args, pio_debounce, pio_dict, message_group_dict, num_pio_bank):
    output_h_file = os.path.basename(os.path.splitext(args.msg_xml)[0] + '.h')
    c_file = "#include \"input_event_manager.h\"\n" + \
             "#include \"" + output_h_file + "\"\n\n" + \
             OutputPioConfigTable(pio_debounce, pio_dict, num_pio_bank) + "\n" + \
             OutputButtonTable(message_group_dict) + "\n"
    print c_file

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Generate input event configuration data  from an XML description of the input events")
    parser.add_argument('--msg_xml', type=str, help='xml file containing the mapping of event messages to button names')
    parser.add_argument('--pio_xml', type=str, help='xml file containing the mapping of PIO pads to button names')
    parser.add_argument('--xsd', type=str, help='xsd file containing XML Schema')
    parser.add_argument('--header', action='store_true')
    parser.add_argument('--source', action='store_true')
    parser.add_argument('--pio_bank', action='store', dest='pio_bank',
                        type=int, help='Number of pio banks on the chip' )
    args = parser.parse_args()

    try:
        if args.xsd:
            for xml in (args.msg_xml, args.pio_xml):
                AutoValidateXML(args.xsd, xml)

        pio_debounce = ParsePioDebounce(args.pio_xml)
        pio_dict = ParsePioXml(args.pio_xml)
        message_group_dict = ParseMessageXml(args.msg_xml, pio_dict)

        if args.header:
            generate_header(args, pio_dict, message_group_dict)

        if args.source:
            generate_source(args, pio_debounce, pio_dict, message_group_dict, args.pio_bank)

    except XmlValidationException as inst:
        sys.stderr.write("ERROR: Generation of input event files failed...\n")
        sys.stderr.write(inst.message + "\n")
