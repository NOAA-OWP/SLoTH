#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define SOURCE_LOC " (" __FILE__ ":" TOSTRING(__LINE__) ")"
// ^ Credit https://www.decompile.com/cpp/faq/file_and_line_error_string.htm

#include "sloth.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <math.h>
#include <stdexcept>
#include <map>
#include <set>
#include <limits>
//#include <iostream>

std::string Sloth::GetComponentName(){
  return "Simple Logical Tautology Handler (SLoTH) Model";
}

double Sloth::GetCurrentTime(){
  return current_model_time;
}

double Sloth::GetEndTime(){
  return std::numeric_limits<double>::max();
}

int Sloth::GetGridRank(const int grid){
  throw std::logic_error("GetGridRank Not implemented." SOURCE_LOC);
}

int Sloth::GetGridSize(const int grid){
  throw std::logic_error("GetGridSize Not implemented." SOURCE_LOC);
}

std::string Sloth::GetGridType(const int grid){
  throw std::logic_error("GetGridType Not implemented." SOURCE_LOC);
}

std::vector<std::string> Sloth::GetInputVarNames(){ //v?
  std::set<std::string> ivars;
  for(auto const& iter: this->var_innames)
    ivars.insert(iter.second);
  return std::vector<std::string>(ivars.begin(), ivars.end());
}
std::vector<std::string> Sloth::GetOutputVarNames(){ //v
  std::vector<std::string> ovars;
  for(auto const& iter: this->var_values)
    ovars.push_back(iter.first); 
  return ovars;
}
int Sloth::GetInputItemCount(){ //v
  return this->GetInputVarNames().size();
}
int Sloth::GetOutputItemCount(){ //v
  return this->var_values.size();
}

double Sloth::GetStartTime(){ //v
  return 0.0;
}

double Sloth::GetTimeStep(){ //v
  return std::numeric_limits<double>::lowest();
}

std::string Sloth::GetTimeUnits(){ //v
  return "s";
}

void Sloth::GetValue(std::string name, void* dest){ //v
  name = this->ResolveInNameAlias(name);

  void *src;
  src = this->GetValuePtr(name);

  int nbytes = this->GetVarNbytes(name);
  std::memcpy (dest, src, nbytes);
}

void Sloth::GetValueAtIndices(std::string name, void* dest, int* inds, int count){ //v
  name = this->ResolveInNameAlias(name);

  if (count < 1)
    throw std::runtime_error(std::string("Illegal count ") + std::to_string(count) + std::string(" provided to GetValueAtIndices(name, dest, inds, count)" SOURCE_LOC));

  void *src;
  src = this->GetValuePtr(name);

  char* srcbyte = (char*)src;
  char* destbyte = (char*)dest;

  int itemsize = this->GetVarItemsize(name);

  for (size_t i = 0; i < count; ++i) {
    std::memcpy(
      destbyte + (itemsize * i), 
      srcbyte + (itemsize * inds[i]), 
      itemsize 
      );
  }
  return;
}

void* Sloth::GetValuePtr(std::string name){ //v
  name = this->ResolveInNameAlias(name);

  auto iter = this->var_values.find(name);
  if(iter != this->var_values.end()){
    return iter->second.get();
  }
  throw std::runtime_error("GetValuePtr called for unknown variable: " + name + "" + SOURCE_LOC );
}

int Sloth::GetVarItemsize(std::string name){ //v
  name = this->ResolveInNameAlias(name);

  std::map<std::string,int>::const_iterator iter = this->type_sizes.find(this->GetVarType(name));
  if(iter != this->type_sizes.end()){
    return iter->second;
  }
  throw std::runtime_error("Item \""+name+"\" has illegal type \""+(this->GetVarType(name))+"\"! " SOURCE_LOC);
}

std::string Sloth::GetVarLocation(std::string name){ //v
  name = this->ProcessNameMeta(name);
  name = this->ResolveInNameAlias(name);

  auto iter = this->var_locations.find(name);
  if(iter != this->var_locations.end()){
    return iter->second;
  }
  throw std::runtime_error("GetVarLocation called for non-existent variable: "+name+" " SOURCE_LOC);
}

int Sloth::GetVarNbytes(std::string name){ //v
  name = this->ProcessNameMeta(name);
  name = this->ResolveInNameAlias(name);

  if(var_nbytes.count(name) <= 0){
    throw std::runtime_error("GetVarNbytes called for non-existent variable "+name+" " SOURCE_LOC);
  }
  int nbytes = var_nbytes[name];
  if(nbytes <= 0){
    return ComputeVarNbytes(name);
  }
  return var_nbytes[name];
}

std::string Sloth::GetVarType(std::string name){ //v
  name = this->ProcessNameMeta(name);
  name = this->ResolveInNameAlias(name);

  auto iter = this->var_types.find(name);
  if(iter != this->var_types.end()){
    return iter->second;
  }
  throw std::runtime_error("GetVarType called for non-existent variable: "+name+" " SOURCE_LOC );
}

/**
 * Determine the appropriate UDUNIT value from a given measurement unit name
 * @param proposed_unit A string representing a unit of measurement
 * @return The proper UDUnit dimensionless measurement if the unit was marked as dimensionless, otherwise the proposed unit
 */
static std::string determine_udunit(const std::string& proposed_unit) {
    std::string lowercase_unit(proposed_unit);

    std::transform(
        lowercase_unit.begin(),
        lowercase_unit.end(),
        lowercase_unit.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );

    if (
        lowercase_unit.empty() ||
        lowercase_unit == "none" ||
        lowercase_unit == "na" ||
        lowercase_unit == "n/a" ||
        lowercase_unit == "-" ||
        lowercase_unit == "null" ||
        lowercase_unit == "unitless"
    ) {
        return "1";
    }

    return proposed_unit;
}

std::string Sloth::GetVarUnits(std::string name){ //v
  name = this->ResolveInNameAlias(name);

  auto iter = this->var_units.find(name);
  if(iter != this->var_units.end()){
    const std::string& unit = iter->second;
    return determine_udunit(unit);
  }
  throw std::runtime_error("GetVarUnits called for non-existent variable: "+name+" " SOURCE_LOC );
}

void Sloth::Initialize(std::string file){ //v
  this->current_model_time = this->GetStartTime();
}

void Sloth::SetValueAtIndices(std::string name, int* inds, int count, void* src){ //v
  if (count < 1)
    throw std::runtime_error(std::string("Illegal count ") + std::to_string(count) + std::string(" provided to SetValueAtIndices(name, dest, inds, count)" SOURCE_LOC));

  // If this is actually destined for an input alias, punt!...
  auto aliases = this->ResolveInNameAliases(name);
  if(!aliases.empty()){
    for(std::string inname : aliases){
      //TODO: Might be worth wrapping this in try/catch and wrapping any exceptions with a note about the actual name tried...
      this->SetValueAtIndices(inname, inds, count, src);
    }
    return;
  }
  // Otherwise...

  void* ptr;

  // If this somehow gets called first, we will need space as if we are setting by value. This *should* never happen.
  name = this->ProcessNameMeta(name);

  void *dest;
  dest = this->GetValuePtr(name);

  
  char* srcbyte = (char*)src;
  char* destbyte = (char*)dest;

  int itemsize = this->GetVarItemsize(name);

  for (size_t i = 0; i < count; ++i) {
    std::memcpy(
      destbyte + (itemsize * inds[i]), 
      srcbyte + (itemsize * i), 
      itemsize 
      );
  }
  return;  
}

void Sloth::SetValue(std::string name, void* src){ //v
  // If this is actually destined for an input alias, punt!...
  auto aliases = this->ResolveInNameAliases(name);
  if(!aliases.empty()){
    for(std::string inname : aliases){
      //TODO: Might be worth wrapping this in try/catch and wrapping any exceptions with a note about the actual name tried...
      this->SetValue(inname, src);
    }
    return;
  }
  // Otherwise...

  name = this->ProcessNameMeta(name);

  void *dest = this->GetValuePtr(name);
  int nbytes = this->GetVarNbytes(name);
  std::memcpy (dest, src, nbytes);
}

void Sloth::Update(){ //v
  this->UpdateUntil(this->current_model_time + this->GetTimeStep());
}

void Sloth::UpdateUntil(double future_time){ //v
  if (this->current_model_time != future_time)
    this->current_model_time = future_time;
}

void Sloth::Finalize(){ //v
  //TODO: Consider resetting state here in case the object is reused?
  return;
}

int Sloth::GetGridEdgeCount(const int grid){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
int Sloth::GetGridFaceCount(const int grid){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridEdgeNodes(const int grid, int* edge_nodes){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridFaceEdges(const int grid, int* face_edges){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridFaceNodes(const int grid, int* face_nodes){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
int Sloth::GetGridNodeCount(const int grid){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridNodesPerFace(const int grid, int* nodes_per_face){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridOrigin(const int grid, double* origin){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridShape(const int grid, int* shape){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridSpacing(const int grid, double* spacing){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridX(const int grid, double* x){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridY(const int grid, double* y){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
void Sloth::GetGridZ(const int grid, double* z){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}
int Sloth::GetVarGrid(std::string name){
  throw std::logic_error("Not implemented." SOURCE_LOC);
}

int Sloth::ComputeVarNbytes(std::string name){ //v
  int item_size = this->GetVarItemsize(name);
  // Note that if this name is not already in state that the above call will throw...
  // so we'll omit checking from here on.

  int item_count;
  std::map<std::string,int>::const_iterator iter = this->var_counts.find(name);
  if(iter != this->var_counts.end()){
    item_count = iter->second;
  }

  return item_size * item_count;
}

/**
 * Return a copy of the given string that is missing all leading and trailing whitespace
 *
 * @param input The input string to trim
 * @return A copy of the input string missing all leading and following whitespace
 */
static std::string trim_whitespace(const std::string& input) {
    size_t first_non_whitespace_index = 0;

    while (first_non_whitespace_index < input.size() && std::isspace(static_cast<unsigned char>(input[first_non_whitespace_index]))) {
        ++first_non_whitespace_index;
    }

    if (first_non_whitespace_index == input.size()) {
        return {};
    }

    size_t last_non_whitespace_index = input.size() - 1;

    while (last_non_whitespace_index > first_non_whitespace_index && std::isspace(static_cast<unsigned char>(input[last_non_whitespace_index]))) {
        --last_non_whitespace_index;
    }

    return input.substr(
        first_non_whitespace_index,
        last_non_whitespace_index - first_non_whitespace_index + 1
    );
}

/**
 * Retrieve the next metadata token embedded within a string of comma delimited values followed by a close parenthesis
 *
 * Say the value for `trimmed_name` is "name(value1,, value3, ,value5)"
 *
 * If:
 *  1. `cursor_position` is `4`, this will return "value1" and move `cursor_position` to `11`
 *  2. `cursor_position` is `11`, this will return "" and move `cursor_position` to `12`
 *  3. `cursor_position` is `12`, this will return "value3" and move `cursor_position` to `20`
 *  4. `cursor_position` is `20`, this will return "" and move `cursor_position` to `22`
 *  5. `cursor_position` is `22`, this will return "value5" and move `cursor_position` to `29`
 * @param trimmed_name An original input name whose leading and trailing whitespace has been trimmed
 * @param position_of_closing_parenthesis The position within the string containing the closing parenthesis
 * @param cursor_position The current position of the cursor that is iterating through the trimmed name
 * @return A token if one was identified
 */
static std::string next_metadata_token(
    const std::string& trimmed_name,
    const size_t position_of_closing_parenthesis,
    size_t& cursor_position
) {
    size_t position_after_token = position_of_closing_parenthesis;
    const size_t comma_position = trimmed_name.find(',', cursor_position + 1);
    const bool comma_found = comma_position != std::string::npos;
    const bool comma_before_closing_parenthesis = comma_found && comma_position < position_of_closing_parenthesis;
    if (comma_before_closing_parenthesis) {
        position_after_token = comma_position;
    }

    std::string token = trimmed_name.substr(
        cursor_position + 1,
        position_after_token - cursor_position - 1
    );
    token = trim_whitespace(token);
    cursor_position = position_after_token;
    return token;
}

std::string Sloth::ProcessNameMeta(std::string nameMaybeWithMeta){ //v
    std::string trimmed_name = trim_whitespace(nameMaybeWithMeta);

    // Early-out: if the name passed is already known, it can be assumed that it has no metadata--return it.
    if(this->var_counts.count(trimmed_name) > 0){
        return trimmed_name;
    }
    // Early-out: if the name passed in is an input alias, will not process metadata, return it.
    if(!this->ResolveInNameAliases(trimmed_name).empty()){
        return trimmed_name;
    }

    // parse clean_name string for metadata
    int count = 1;
    std::string type = "double";
    std::string units = "1";
    std::string location = "node";
    std::string input_name;

    // parse clean_name string for metadata
    std::string raw_name;

    /* Represents the index of the first character in a token being investigated */
    size_t cursor_position = trimmed_name.find('(');
    const bool parenthesis_found = cursor_position != std::string::npos;

    if(parenthesis_found) {
        /* The index of the last character in a token being investigated */
        const size_t position_of_close_parenthesis = trimmed_name.find(')', cursor_position + 1);
        const bool missing_closing_parenthesis = position_of_close_parenthesis == std::string::npos;

        if(missing_closing_parenthesis){
            throw std::runtime_error(
                "Missing closing paren in variable definition '" + trimmed_name + "' " + SOURCE_LOC
            );
        }

        // Get the raw clean_name being processed - it will be the text up to the identified opening parenthesis
        raw_name = trimmed_name.substr(0, cursor_position);
        raw_name = trim_whitespace(raw_name);

        const std::string count_representation = next_metadata_token(
            trimmed_name,
            position_of_close_parenthesis,
            cursor_position
        );
        const bool has_count = !count_representation.empty();

        // Look for the first value - if content was found between the cursor position and the end token, we've found
        // what must be a count
        if(has_count) {
            try {
                count = std::stoi(count_representation);
            } catch (const std::exception&) {
                throw std::runtime_error(
                    std::string("Sloth::ProcessNameMeta - The identified value for the count ('") +
                    count_representation + "') within '" + nameMaybeWithMeta + "' is invalid. It must be an integer." +
                    SOURCE_LOC
                );
            }
        }

        const std::string variable_type = next_metadata_token(
            trimmed_name,
            position_of_close_parenthesis,
            cursor_position
        );
        const bool has_variable_type = !variable_type.empty();

        if(has_variable_type) {
            type = variable_type;
            if(type_sizes.count(type) <= 0){
                throw std::runtime_error("Illegal type '" + type + "' specified for '" + nameMaybeWithMeta + "' " SOURCE_LOC);
            }
        }

        const std::string units_token = next_metadata_token(
            trimmed_name,
            position_of_close_parenthesis,
            cursor_position
        );
        const bool has_units_token = !units_token.empty();

        if(has_units_token){
            units = units_token;
            units = determine_udunit(units);
        }

        const std::string location_token = next_metadata_token(
            trimmed_name,
            position_of_close_parenthesis,
            cursor_position
        );
        const bool has_location_token = !location_token.empty();

        if(has_location_token){
            location = location_token;
        }

        const std::string input_name_token = next_metadata_token(
            trimmed_name,
            position_of_close_parenthesis,
            cursor_position
        );
        const bool has_input_name_token = !input_name_token.empty();

        if(has_input_name_token) {
            input_name = input_name_token;
        }

        if (!input_name.empty()) {
            // Validate non-collision for input_name
            if(input_name == raw_name){
                throw std::runtime_error(
                    "Aliasing an input variable (\"" + input_name + "\") to its own name is not allowed!"
                );
            }

            if(this->var_values.count(input_name) > 0) {
                throw std::runtime_error(
                    "Attempt to set input alias \"" + input_name +
                    "\" for variable \"" + raw_name +
                    "\" conflicts with existing output variable of the same name, which is not allowed!"
                );
            }
        }
    } else {
        raw_name = trimmed_name;
    }

    // If this is a new name, make sure it does not collide with a previously defined input alias
    // (Checking first if it is new is an optimization)
    if(this->var_counts.count(raw_name) <= 0 && !this->ResolveInNameAliases(raw_name).empty()){
        throw std::runtime_error(
            "Attempt to define a new variable \"" +
            raw_name +
            "\" which conflicts with a previously defined input alias of the same name, which is not allowed!"
        );
    }

    var_units[raw_name] = units;
    var_counts[raw_name] = count;
    var_types[raw_name] = type;
    var_locations[raw_name] = location;

    if(!input_name.empty()){
        var_innames[raw_name] = input_name;
    }

    this->EnsureAllocatedForByValue(raw_name);

    return raw_name;
}

std::string Sloth::ResolveInNameAlias(std::string name){
  //TODO: Make a shortcut reverse map for this case?
  std::string retval = name;
  for (auto iter = this->var_innames.begin(); iter != this->var_innames.end(); ++iter)
    if (iter->second == name){
      retval = iter->first;
      break;
    }
  return retval;
}

std::vector<std::string> Sloth::ResolveInNameAliases(std::string name){
  std::vector<std::string> retval;
  for (auto iter = this->var_innames.begin(); iter != this->var_innames.end(); ++iter)
    if (iter->second == name)
      retval.push_back(iter->first);
  return retval;
}

void Sloth::EnsureAllocatedForByValue(std::string name){
  int nbytes = this->var_nbytes[name]; // Note that this will set a default of zero!
  if(nbytes == 0){
    // New varaible! We are setting by value, so set up some memory we will own...
    //TODO: May need some try/catch/finally to ensure consistency between var_nbytes and var_values?
    nbytes = this->var_nbytes[name] = this->ComputeVarNbytes(name);
    this->var_values.emplace(name, std::shared_ptr<void>(std::malloc(nbytes), std::free));
  }
}