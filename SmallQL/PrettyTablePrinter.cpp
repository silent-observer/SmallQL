#include "PrettyTablePrinter.h"

#include <sstream>
#include <iomanip>

string stringifyValue(const Value& v) {
	switch (v.type)
	{
	case ValueType::Null: return "NULL";
	case ValueType::MaxVal: return "+INF";
	case ValueType::MinVal: return "-INF";
	case ValueType::Integer: return to_string(v.intVal);
	case ValueType::Double: return to_string(v.doubleVal);
	case ValueType::Datetime: return v.datetimeVal.toString();
	case ValueType::String: return v.stringVal;
	}
}

void printLine(ostream& ss, const vector<int>& maxWidth) {
	ss << "+";
	for (int col = 0; col < maxWidth.size(); col++) {
		ss << "-" << string(maxWidth[col], '-') << "-+";
	}
	ss << endl;
}

string prettyPrintTable(const vector<ValueArray>& table, const IntermediateType& type) {
	if (table.size() == 0) return "<empty set>\n";
	vector<string> columnNames;
	vector<vector<string>> data;
	vector<int> maxWidth;
	for (const auto& e : type.entries) {
		columnNames.push_back(e.columnName);
		maxWidth.push_back(e.columnName.size());
	}
	for (const auto& row : table) {
		data.emplace_back(row.size(), "");
		for (int i = 0; i < row.size(); i++) {
			string s = stringifyValue(row[i]);
			data.back()[i] = s;
			maxWidth[i] = max(maxWidth[i], (int)s.size());
		}
	}
	stringstream ss;

	// Top line
	printLine(ss, maxWidth);
	// Column names
	ss << "|";
	for (int col = 0; col < columnNames.size(); col++) {
		ss << " " << setw(maxWidth[col]) << columnNames[col] << " |";
	}
	ss << endl;
	// Middle line
	printLine(ss, maxWidth);
	// Data
	for (const auto& row : data) {
		ss << "|";
		for (int col = 0; col < columnNames.size(); col++) {
			ss << " " << setw(maxWidth[col]) << row[col] << " |";
		}
		ss << endl;
	}
	// Bottom line
	printLine(ss, maxWidth);
	return ss.str();
}