import struct
import xml.etree.ElementTree as ET

def write_utf_string(f, s):
    # Format DataOutputStream: 2 byte lenght + string
    binary_str = s.encode('utf-8')
    f.write(struct.pack('>H', len(binary_str)))
    f.write(binary_str)

def convert_xml_to_col(xml_file, output_col):
    try:
        tree = ET.parse(xml_file)
        root = tree.getroot()
        
        colours = []
        for colour in root.findall('colour'):
            name = colour.get('name')
            value_str = colour.get('value')
            
            # Convert hex to int
            value_int = int(value_str, 16)
            colours.append((name, value_int))
        
        with open(output_col, 'wb') as f:
            # Header: Version1 + colour counter
            f.write(struct.pack('>I', 1)) 
            f.write(struct.pack('>I', len(colours)))
            
            for name, value in colours:
                write_utf_string(f, name)
                # 'I' (unsigned int) to avoid error 2 miliardi
                f.write(struct.pack('>I', value))
                
        print(f"Success! Created: {output_col} with {len(colours)} colors.")
        
    except Exception as e:
        print(f"Error during conversion: {e}")

if __name__ == "__main__":
    # Make sure file name is  "colours.xml"
    convert_xml_to_col('colours.xml', 'colours.col')