/*****************************************************************
 * @file YamlParser.hpp
 * @brief Lightweight YAML Parser for Embedded Systems
 * 
 * A minimal YAML parser designed for ESP32 scene configuration.
 * Supports a subset of YAML:
 * - Scalar values (strings, integers, floats, booleans)
 * - Nested maps (key: value)
 * - Arrays (- item)
 * - Comments (# comment)
 * 
 * Design principles:
 * - Low memory footprint
 * - No external dependencies (std::string, std::vector, std::map only)
 * - Single header implementation
 * - Parse and serialize support
 * 
 * Usage:
 *   #include "Drivers/YamlParser.hpp"
 *   
 *   // Parse YAML string
 *   YamlNode root = YamlParser::parse(yamlString);
 *   
 *   // Access values
 *   std::string name = root["name"].asString();
 *   int id = root["id"].asInt();
 *   float speed = root["animation"]["speed"].asFloat();
 *   bool enabled = root["enabled"].asBool();
 *   
 *   // Access array
 *   for (size_t i = 0; i < root["items"].size(); i++) {
 *     auto& item = root["items"][i];
 *   }
 *   
 *   // Serialize back to YAML
 *   std::string yaml = YamlParser::serialize(root);
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#ifndef ARCOS_DRIVERS_YAML_PARSER_HPP_
#define ARCOS_DRIVERS_YAML_PARSER_HPP_

#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>

namespace Drivers {

//=============================================================================
// Forward Declarations
//=============================================================================

class YamlNode;

//=============================================================================
// YamlNode - Represents a YAML value (scalar, map, or array)
//=============================================================================

class YamlNode {
public:
    enum class Type {
        NUL,      // null/undefined
        SCALAR,   // string, int, float, bool
        MAP,      // key-value pairs
        ARRAY     // list of values
    };
    
private:
    Type type_;
    std::string scalar_;                        // For SCALAR type
    std::map<std::string, YamlNode> map_;       // For MAP type
    std::vector<YamlNode> array_;               // For ARRAY type
    
public:
    //=========================================================================
    // Constructors
    //=========================================================================
    
    YamlNode() : type_(Type::NUL) {}
    
    explicit YamlNode(const std::string& value) 
        : type_(Type::SCALAR), scalar_(value) {}
    
    explicit YamlNode(const char* value) 
        : type_(Type::SCALAR), scalar_(value ? value : "") {}
    
    explicit YamlNode(int value) 
        : type_(Type::SCALAR) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", value);
        scalar_ = buf;
    }
    
    explicit YamlNode(float value) 
        : type_(Type::SCALAR) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.6g", value);
        scalar_ = buf;
    }
    
    explicit YamlNode(double value) 
        : type_(Type::SCALAR) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.6g", value);
        scalar_ = buf;
    }
    
    explicit YamlNode(bool value) 
        : type_(Type::SCALAR), scalar_(value ? "true" : "false") {}
    
    // Copy constructor and assignment
    YamlNode(const YamlNode& other) = default;
    YamlNode& operator=(const YamlNode& other) = default;
    
    // Move constructor and assignment
    YamlNode(YamlNode&& other) noexcept = default;
    YamlNode& operator=(YamlNode&& other) noexcept = default;
    
    //=========================================================================
    // Type Accessors
    //=========================================================================
    
    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::NUL; }
    bool isScalar() const { return type_ == Type::SCALAR; }
    bool isMap() const { return type_ == Type::MAP; }
    bool isArray() const { return type_ == Type::ARRAY; }
    
    //=========================================================================
    // Value Accessors
    //=========================================================================
    
    /** Get as string (returns empty string if not scalar) */
    std::string asString(const std::string& defaultVal = "") const {
        if (type_ != Type::SCALAR) return defaultVal;
        return scalar_;
    }
    
    /** Get as integer */
    int asInt(int defaultVal = 0) const {
        if (type_ != Type::SCALAR || scalar_.empty()) return defaultVal;
        return std::atoi(scalar_.c_str());
    }
    
    /** Get as float */
    float asFloat(float defaultVal = 0.0f) const {
        if (type_ != Type::SCALAR || scalar_.empty()) return defaultVal;
        return static_cast<float>(std::atof(scalar_.c_str()));
    }
    
    /** Get as double */
    double asDouble(double defaultVal = 0.0) const {
        if (type_ != Type::SCALAR || scalar_.empty()) return defaultVal;
        return std::atof(scalar_.c_str());
    }
    
    /** Get as boolean */
    bool asBool(bool defaultVal = false) const {
        if (type_ != Type::SCALAR) return defaultVal;
        std::string lower = scalar_;
        for (char& c : lower) c = std::tolower(c);
        if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") return true;
        if (lower == "false" || lower == "no" || lower == "off" || lower == "0") return false;
        return defaultVal;
    }
    
    //=========================================================================
    // Map Access (key-value)
    //=========================================================================
    
    /** Access map value by key (creates if doesn't exist) */
    YamlNode& operator[](const std::string& key) {
        if (type_ == Type::NUL) {
            type_ = Type::MAP;
        }
        if (type_ != Type::MAP) {
            static YamlNode null_node;
            return null_node;
        }
        return map_[key];
    }
    
    /** Access map value by key (const version) */
    const YamlNode& operator[](const std::string& key) const {
        static YamlNode null_node;
        if (type_ != Type::MAP) return null_node;
        auto it = map_.find(key);
        if (it == map_.end()) return null_node;
        return it->second;
    }
    
    /** Access map value by C-string key */
    YamlNode& operator[](const char* key) {
        return operator[](std::string(key));
    }
    
    const YamlNode& operator[](const char* key) const {
        return operator[](std::string(key));
    }
    
    /** Check if map contains key */
    bool hasKey(const std::string& key) const {
        if (type_ != Type::MAP) return false;
        return map_.find(key) != map_.end();
    }
    
    /** Get all keys in map */
    std::vector<std::string> keys() const {
        std::vector<std::string> result;
        if (type_ == Type::MAP) {
            for (const auto& kv : map_) {
                result.push_back(kv.first);
            }
        }
        return result;
    }
    
    /** Get map reference */
    const std::map<std::string, YamlNode>& asMap() const { return map_; }
    std::map<std::string, YamlNode>& asMap() { return map_; }
    
    //=========================================================================
    // Array Access
    //=========================================================================
    
    /** Access array element by index */
    YamlNode& operator[](size_t index) {
        if (type_ == Type::NUL) {
            type_ = Type::ARRAY;
        }
        if (type_ != Type::ARRAY) {
            static YamlNode null_node;
            return null_node;
        }
        if (index >= array_.size()) {
            array_.resize(index + 1);
        }
        return array_[index];
    }
    
    const YamlNode& operator[](size_t index) const {
        static YamlNode null_node;
        if (type_ != Type::ARRAY || index >= array_.size()) {
            return null_node;
        }
        return array_[index];
    }
    
    /** Get array size */
    size_t size() const {
        if (type_ == Type::ARRAY) return array_.size();
        if (type_ == Type::MAP) return map_.size();
        return 0;
    }
    
    /** Add element to array */
    void push(const YamlNode& node) {
        if (type_ == Type::NUL) {
            type_ = Type::ARRAY;
        }
        if (type_ == Type::ARRAY) {
            array_.push_back(node);
        }
    }
    
    /** Get array reference */
    const std::vector<YamlNode>& asArray() const { return array_; }
    std::vector<YamlNode>& asArray() { return array_; }
    
    //=========================================================================
    // Modifiers
    //=========================================================================
    
    /** Set as scalar value */
    void setScalar(const std::string& value) {
        type_ = Type::SCALAR;
        scalar_ = value;
        map_.clear();
        array_.clear();
    }
    
    /** Make this node a map */
    void makeMap() {
        if (type_ != Type::MAP) {
            type_ = Type::MAP;
            scalar_.clear();
            array_.clear();
        }
    }
    
    /** Make this node an array */
    void makeArray() {
        if (type_ != Type::ARRAY) {
            type_ = Type::ARRAY;
            scalar_.clear();
            map_.clear();
        }
    }
    
    /** Clear the node */
    void clear() {
        type_ = Type::NUL;
        scalar_.clear();
        map_.clear();
        array_.clear();
    }
};

//=============================================================================
// YamlParser - Static parsing and serialization functions
//=============================================================================

class YamlParser {
public:
    
    //=========================================================================
    // Parse YAML string into YamlNode tree
    //=========================================================================
    
    static YamlNode parse(const std::string& yaml) {
        std::vector<std::string> lines;
        splitLines(yaml, lines);
        
        YamlNode root;
        root.makeMap();
        
        size_t lineIdx = 0;
        parseBlock(lines, lineIdx, 0, root);
        
        return root;
    }
    
    static YamlNode parse(const char* yaml) {
        return parse(std::string(yaml ? yaml : ""));
    }
    
    //=========================================================================
    // Serialize YamlNode tree to YAML string
    //=========================================================================
    
    static std::string serialize(const YamlNode& node, int indent = 0) {
        std::string result;
        serializeNode(node, result, indent, false);
        return result;
    }
    
private:
    //=========================================================================
    // Parsing Helpers
    //=========================================================================
    
    /** Split input into lines */
    static void splitLines(const std::string& yaml, std::vector<std::string>& lines) {
        size_t start = 0;
        size_t end;
        while ((end = yaml.find('\n', start)) != std::string::npos) {
            lines.push_back(yaml.substr(start, end - start));
            start = end + 1;
        }
        if (start < yaml.length()) {
            lines.push_back(yaml.substr(start));
        }
    }
    
    /** Get indentation level (number of leading spaces / 2) */
    static int getIndent(const std::string& line) {
        int spaces = 0;
        for (char c : line) {
            if (c == ' ') spaces++;
            else if (c == '\t') spaces += 2;
            else break;
        }
        return spaces / 2;
    }
    
    /** Trim whitespace from both ends */
    static std::string trim(const std::string& s) {
        size_t start = 0;
        size_t end = s.length();
        while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) start++;
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
        return s.substr(start, end - start);
    }
    
    /** Remove quotes from string if present */
    static std::string unquote(const std::string& s) {
        if (s.length() >= 2) {
            if ((s.front() == '"' && s.back() == '"') ||
                (s.front() == '\'' && s.back() == '\'')) {
                return s.substr(1, s.length() - 2);
            }
        }
        return s;
    }
    
    /** Check if line is empty or comment */
    static bool isEmptyOrComment(const std::string& line) {
        std::string trimmed = trim(line);
        return trimmed.empty() || trimmed[0] == '#';
    }
    
    /** Check if line is an array item (starts with -) */
    static bool isArrayItem(const std::string& line) {
        std::string trimmed = trim(line);
        return !trimmed.empty() && trimmed[0] == '-';
    }
    
    /** Parse a key-value or array item line
     * Returns: key (empty for array items), value (may be empty if nested block follows)
     */
    static bool parseLine(const std::string& line, std::string& key, std::string& value, bool& isArray) {
        std::string trimmed = trim(line);
        
        // Skip empty and comments
        if (trimmed.empty() || trimmed[0] == '#') {
            return false;
        }
        
        isArray = false;
        
        // Array item
        if (trimmed[0] == '-') {
            isArray = true;
            key.clear();
            
            // Get content after -
            std::string after = trim(trimmed.substr(1));
            
            // Check if it's "- key: value" format
            size_t colonPos = after.find(':');
            if (colonPos != std::string::npos) {
                // This is a map inside array
                value.clear();  // Signal nested content
            } else {
                // Simple array value
                value = unquote(after);
            }
            return true;
        }
        
        // Key-value pair
        size_t colonPos = trimmed.find(':');
        if (colonPos == std::string::npos) {
            // Not a valid key-value, treat as continuation or error
            return false;
        }
        
        key = trim(trimmed.substr(0, colonPos));
        value = trim(trimmed.substr(colonPos + 1));
        
        // Remove inline comments
        size_t commentPos = value.find(" #");
        if (commentPos != std::string::npos) {
            value = trim(value.substr(0, commentPos));
        }
        
        // Unquote the value
        value = unquote(value);
        
        return true;
    }
    
    /** Parse a block of YAML at given indent level */
    static void parseBlock(const std::vector<std::string>& lines, size_t& lineIdx, 
                          int expectedIndent, YamlNode& parent) {
        while (lineIdx < lines.size()) {
            const std::string& line = lines[lineIdx];
            
            // Skip empty lines and comments
            if (isEmptyOrComment(line)) {
                lineIdx++;
                continue;
            }
            
            int indent = getIndent(line);
            
            // If we've outdented, we're done with this block
            if (indent < expectedIndent) {
                return;
            }
            
            // Parse the line
            std::string key, value;
            bool isArray;
            
            if (!parseLine(line, key, value, isArray)) {
                lineIdx++;
                continue;
            }
            
            if (isArray) {
                // Array item
                if (parent.type() == YamlNode::Type::NUL) {
                    parent.makeArray();
                }
                
                // Get content after the dash
                std::string trimmed = trim(line);
                std::string afterDash = trim(trimmed.substr(1));
                
                // Check if it's a nested map "- key: value"
                size_t colonPos = afterDash.find(':');
                if (colonPos != std::string::npos) {
                    // Nested map in array
                    YamlNode itemNode;
                    itemNode.makeMap();
                    
                    // Parse this line as key: value
                    std::string itemKey = trim(afterDash.substr(0, colonPos));
                    std::string itemValue = trim(afterDash.substr(colonPos + 1));
                    
                    // Remove comments
                    size_t commentPos = itemValue.find(" #");
                    if (commentPos != std::string::npos) {
                        itemValue = trim(itemValue.substr(0, commentPos));
                    }
                    itemValue = unquote(itemValue);
                    
                    if (itemValue.empty()) {
                        // Nested block
                        lineIdx++;
                        YamlNode nestedNode;
                        parseBlock(lines, lineIdx, indent + 1, nestedNode);
                        itemNode[itemKey] = nestedNode;
                    } else {
                        itemNode[itemKey] = YamlNode(itemValue);
                        lineIdx++;
                    }
                    
                    // Check for more keys at same level
                    while (lineIdx < lines.size()) {
                        const std::string& nextLine = lines[lineIdx];
                        if (isEmptyOrComment(nextLine)) {
                            lineIdx++;
                            continue;
                        }
                        int nextIndent = getIndent(nextLine);
                        if (nextIndent <= indent) break;
                        
                        std::string nk, nv;
                        bool na;
                        if (parseLine(nextLine, nk, nv, na) && !na && !nk.empty()) {
                            if (nv.empty()) {
                                lineIdx++;
                                YamlNode nestedNode;
                                parseBlock(lines, lineIdx, nextIndent + 1, nestedNode);
                                itemNode[nk] = nestedNode;
                            } else {
                                itemNode[nk] = YamlNode(nv);
                                lineIdx++;
                            }
                        } else {
                            break;
                        }
                    }
                    
                    parent.push(itemNode);
                } else {
                    // Simple value in array
                    parent.push(YamlNode(unquote(afterDash)));
                    lineIdx++;
                }
            } else {
                // Key-value pair
                if (parent.type() == YamlNode::Type::NUL) {
                    parent.makeMap();
                }
                
                if (value.empty()) {
                    // Nested block follows
                    lineIdx++;
                    YamlNode childNode;
                    parseBlock(lines, lineIdx, indent + 1, childNode);
                    parent[key] = childNode;
                } else {
                    // Simple value
                    parent[key] = YamlNode(value);
                    lineIdx++;
                }
            }
        }
    }
    
    //=========================================================================
    // Serialization Helpers
    //=========================================================================
    
    static void serializeNode(const YamlNode& node, std::string& result, 
                             int indent, bool isArrayItem) {
        std::string indentStr(indent * 2, ' ');
        
        switch (node.type()) {
            case YamlNode::Type::NUL:
                if (!isArrayItem) {
                    result += "null\n";
                }
                break;
                
            case YamlNode::Type::SCALAR:
                {
                    std::string val = node.asString();
                    // Check if value needs quoting
                    bool needsQuotes = false;
                    if (val.empty()) needsQuotes = true;
                    else if (val.find(':') != std::string::npos ||
                             val.find('#') != std::string::npos ||
                             val.find('\n') != std::string::npos ||
                             val.find('"') != std::string::npos ||
                             val[0] == ' ' || val[val.length()-1] == ' ' ||
                             val[0] == '-' || val[0] == '[' || val[0] == '{') {
                        needsQuotes = true;
                    }
                    
                    if (needsQuotes) {
                        result += "\"";
                        // Escape special characters
                        for (char c : val) {
                            if (c == '"') result += "\\\"";
                            else if (c == '\\') result += "\\\\";
                            else if (c == '\n') result += "\\n";
                            else result += c;
                        }
                        result += "\"\n";
                    } else {
                        result += val + "\n";
                    }
                }
                break;
                
            case YamlNode::Type::MAP:
                if (isArrayItem && !node.asMap().empty()) {
                    result += "\n";
                }
                for (const auto& kv : node.asMap()) {
                    result += indentStr + kv.first + ": ";
                    if (kv.second.type() == YamlNode::Type::MAP ||
                        kv.second.type() == YamlNode::Type::ARRAY) {
                        result += "\n";
                        serializeNode(kv.second, result, indent + 1, false);
                    } else {
                        serializeNode(kv.second, result, indent, false);
                    }
                }
                break;
                
            case YamlNode::Type::ARRAY:
                for (const auto& item : node.asArray()) {
                    result += indentStr + "- ";
                    if (item.type() == YamlNode::Type::MAP) {
                        // Inline first key-value, rest indented
                        const auto& map = item.asMap();
                        if (!map.empty()) {
                            auto it = map.begin();
                            result += it->first + ": ";
                            if (it->second.type() == YamlNode::Type::MAP ||
                                it->second.type() == YamlNode::Type::ARRAY) {
                                result += "\n";
                                serializeNode(it->second, result, indent + 2, false);
                            } else {
                                serializeNode(it->second, result, indent + 1, false);
                            }
                            // Remaining keys
                            ++it;
                            for (; it != map.end(); ++it) {
                                result += indentStr + "  " + it->first + ": ";
                                if (it->second.type() == YamlNode::Type::MAP ||
                                    it->second.type() == YamlNode::Type::ARRAY) {
                                    result += "\n";
                                    serializeNode(it->second, result, indent + 2, false);
                                } else {
                                    serializeNode(it->second, result, indent + 1, false);
                                }
                            }
                        } else {
                            result += "{}\n";
                        }
                    } else if (item.type() == YamlNode::Type::ARRAY) {
                        result += "\n";
                        serializeNode(item, result, indent + 1, false);
                    } else {
                        serializeNode(item, result, indent, true);
                    }
                }
                break;
        }
    }
};

} // namespace Drivers

#endif // ARCOS_DRIVERS_YAML_PARSER_HPP_
