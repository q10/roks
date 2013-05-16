void extractFileExtension(string &format, const string &fileName) {
    size_t npos = fileName.find_last_of(".");
    if(npos != std::string::npos) format = fileName.substr(npos+1);    
}

void printMatrix(vector<double> &matrix, unsigned int rows, unsigned int columns, bool columnMajorOrder = true) {
    if (matrix.size() != rows * columns) { cerr << "ERROR: INCORRECT MATCHING OF ROWS AND COLUMNS WITH ACTUAL VECTOR SIZE; EXITING" << endl; abort(); }
    if (columnMajorOrder) {
        for (unsigned int i=0; i < rows; i++) {
            for (unsigned int j=i; j < matrix.size(); j+=rows)
                cout << matrix[j] << " ";                
            cout << endl;
        }
    } else {
        for (unsigned int i=0; i < matrix.size(); i+=columns) {
            for (unsigned int j=0; j < columns; j++)
                cout << matrix[i + j] << " ";
            cout << endl;
        }
    }
}

unsigned int importMoleculesFromFile(vector<OBMol> &moleculesList, const string &fileName) { // does not clear list, only appends new molecules to it
    string format; extractFileExtension(format, fileName);
    OBConversion obconversion;
    if (not obconversion.SetInFormat(format.c_str())) { cerr << "ERROR: OpenBabel does not recognize the following format: '" << format << "'; exiting" << endl; abort(); }

    unsigned int numMoleculesInFile = 0;
    OBMol mol; bool notAtEnd = obconversion.ReadFile(&mol, fileName.c_str());
    while (notAtEnd) {
        numMoleculesInFile++;
        moleculesList.push_back(mol);
        mol.Clear(); notAtEnd = obconversion.Read(&mol);
    }
    return numMoleculesInFile;
}

void writeMoleculeToFile(const string &fileName, OBMol &molecule, bool rewriteFile=false) {
    string format; extractFileExtension(format, fileName);

    ios_base::openmode fileMode = rewriteFile ? ios::out : (ios::out|ios::app);
    std::ofstream ofs(fileName.c_str(), fileMode);
    
    OBConversion obconversion;
    if (not obconversion.SetOutFormat(format.c_str())) { 
        cerr << "WARNING: OpenBabel does not recognize the following format: '" << format << "'; will write to SDF format" << endl;
        obconversion.SetOutFormat("sdf");
    }
    cout << "WRITING MOLECULE TO FILE '" << fileName << "'";
    if (rewriteFile) cout << " (WILL OVERWRITE EXISTING FILE IF ANY)...\n";
    else cout << "...\n";
    obconversion.Write(&molecule, &ofs);  // obconversion.WriteFile(&molecule, fileName.c_str());
}

// currently takes the first molecule in file, and if sequential molecules have different formulas, simply disposes of it
void importMoleculeConformersFromFile(vector<OBMol> &moleculesList, const string &fileName) {
    vector<OBMol> tempList;
    importMoleculesFromFile(tempList, fileName);
    OBMol *tempMoleculeBuild = new OBMol(tempList[0]);
    for (unsigned int i=1; i < tempList.size(); i++) {
        if (tempMoleculeBuild->GetFormula().compare( tempList[i].GetFormula() ) == 0) tempMoleculeBuild->AddConformer(tempList[i].GetCoordinates());
    }
    moleculesList.push_back(*tempMoleculeBuild);
}

void writeMoleculeConformersToFile(const string &fileName, OBMol &molecule, bool rewriteFile=false) {
    for (int i = molecule.NumConformers()-1; i >= 0; i--) { // DO NOT USE UNSIGNED INT i!!!
        molecule.SetConformer(i);
        writeMoleculeToFile(fileName, molecule, rewriteFile);
        rewriteFile = false; // if rewriteFile flag is on, keep it on only for the first conformer's write
    }
}

void printMoleculeCoords(OBMol &molecule) {
    for (OBAtomIterator iter = molecule.BeginAtoms(); iter != molecule.EndAtoms(); iter++) cout << (*iter)->GetVector() << endl;
}

void generateCoordsMatrixFromMolecule(vector<double> &matrix, OBMol &molecule) {     // generates column-order matrix of coordinates
    matrix.clear(); matrix.insert(matrix.end(), molecule.GetCoordinates(), &molecule.GetCoordinates()[3*molecule.NumAtoms()]);
}

void saveCoordsMatrixToMolecule(OBMol &molecule, vector<double> &matrix) {
    if (matrix.size() != molecule.NumAtoms() * 3) { cerr << "ERROR: INCORRECT MATCHING OF NUMBER OF COORDINATES; EXITING" << endl; abort(); }
    molecule.SetCoordinates(&matrix[0]);
}

void writeTemporaryMoleculeCoordsToFile(const string &fileName, OBMol &molecule, vector<double> &tempCoords, bool rewriteFile=false) {
    vector<double> oldCoords;
    generateCoordsMatrixFromMolecule(oldCoords, molecule);
    saveCoordsMatrixToMolecule(molecule, tempCoords);
    writeMoleculeToFile(fileName, molecule, rewriteFile);
    saveCoordsMatrixToMolecule(molecule, oldCoords);
}

void generateAtomicNumbersListFromMolecule(vector<int> &numList, OBMol &molecule) {
    numList.clear();
    for (OBAtomIterator iter = molecule.BeginAtoms(); iter != molecule.EndAtoms(); iter++)
        numList.push_back((*iter)->GetAtomicNum());
}

void getMoleculeCenterCoords(vector<double> &centerCoords, OBMol &molecule) {
    centerCoords.clear(); centerCoords.resize(3, 0);
    for (OBAtomIterator iter = molecule.BeginAtoms(); iter != molecule.EndAtoms(); iter++) {
        double *tmpCoords = (*iter)->GetCoordinate(); double tmpMass = (*iter)->GetAtomicMass();  
        centerCoords[0] += tmpMass*tmpCoords[0]; centerCoords[1] += tmpMass*tmpCoords[1]; centerCoords[2] += tmpMass*tmpCoords[2];
    }
    for (unsigned int i=0; i < centerCoords.size(); i++) centerCoords[i] /= molecule.GetMolWt();
}

void printMoleculeCenterCoords(OBMol &molecule) {
    vector<double> centerCoords; getMoleculeCenterCoords(centerCoords, molecule);
    cout << "CENTER COORDS: ";
    for (unsigned int i=0; i < centerCoords.size(); i++) { cout << centerCoords[i] << " "; }
    cout << endl;
}

inline void translate3DMatrixCoordinates(vector<double> &matrix, double x, double y, double z) {
    for (unsigned int i=0; i < matrix.size(); i+=3) { matrix[i] += x; matrix[i+1] += y; matrix[i+2] += z; }
}

void rotate3DMatrixCoordinates(vector<double> &matrix, vector<double> &rotationMatrix) {
    vector<double> resultMatrix(matrix.size(), 0); // both matrices must be of column-order
    cblas_dgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, 3, matrix.size()/3, 3, 1, &rotationMatrix[0], 3, &matrix[0], 3, 0, &resultMatrix[0], 3);
    matrix = resultMatrix;
}

void removeNonBondedAtomsInMolecule(OBMol &molecule) {
    cout << "Deleting unbonded atoms... "; int numDeleted = 0;
    for (OBAtomIterator iter = molecule.BeginAtoms(); iter != molecule.EndAtoms(); iter++) {
        OBAtom *currentAtom = *iter;
        if ((not currentAtom->HasSingleBond()) and (not currentAtom->HasNonSingleBond()) and molecule.DeleteAtom(currentAtom)) numDeleted++;
    }
    cout << "deleted " << numDeleted << " unbonded atoms." << endl;
}
